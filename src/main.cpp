#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <algorithm>

using namespace geode::prelude;

namespace {
    struct Stage { int all; int wave; };  // in ticks; 4 ticks = 1 frame @ 60fps
    constexpr Stage STAGES[] = {
        {2, 2}, {4, 4}, {8, 8}, {16, 8}, {24, 24},
        {32, 32}, {48, 48}, {54, 54}, {60, 60}, {0, 0}
    };
    constexpr int STAGE_COUNT = 10;
    constexpr float TICK = 1.f / 240.f;

    int stageIndex() {
        auto v = static_cast<int>(Mod::get()->getSettingValue<int64_t>("stage"));
        return std::clamp(v, 1, STAGE_COUNT) - 1;
    }

    std::string stageLabel() {
        int i = stageIndex();
        if (STAGES[i].all == 0) return "Buffer: OFF";
        return fmt::format("Buffer: {} ({} ticks)", i + 1, STAGES[i].all);
    }

    struct Side {
        float lastPress = -1000.f;
        bool held = false;
        bool wasOnGround = true;
        bool pendingRelease = false;
    };
}

class $modify(BufferGJBGL, GJBaseGameLayer) {
    struct Fields {
        float now = 0.f;
        Side sides[2];
    };

    void handleButton(bool down, int button, bool isPlayer1) {
        if (button == 1) {
            auto& s = m_fields->sides[isPlayer1 ? 0 : 1];
            if (down) { s.lastPress = m_fields->now; s.held = true; }
            else { s.held = false; }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void update(float dt) {
        auto f = m_fields.self();
        f->now += dt;

        // finish a synthetic tap started last frame
        for (int i = 0; i < 2; i++) {
            auto& s = f->sides[i];
            if (s.pendingRelease) {
                s.pendingRelease = false;
                if (!s.held) GJBaseGameLayer::handleButton(false, 1, i == 0);
            }
        }

        GJBaseGameLayer::update(dt);

        bool on = Mod::get()->getSettingValue<bool>("buffer-on");
        int window = STAGES[stageIndex()].all;

        for (int i = 0; i < 2; i++) {
            auto& s = f->sides[i];
            PlayerObject* p = i == 0 ? m_player1 : m_player2;
            if (!p) continue;

            bool ground = p->m_isOnGround;
            bool landed = ground && !s.wasOnGround;
            s.wasOnGround = ground;

            // Only click-to-jump modes. Wave/ship/ufo/ball/swing are not buffered here.
            if (p->m_isDart || p->m_isShip || p->m_isBird || p->m_isBall || p->m_isSwing) continue;

            if (on && window > 0 && landed && !s.held && s.lastPress > -999.f
                && (f->now - s.lastPress) <= window * TICK) {
                s.lastPress = -1000.f;
                s.pendingRelease = true;
                GJBaseGameLayer::handleButton(true, 1, i == 0);
            }
        }

        // TODO(wave): STAGES[..].wave is reserved for a wave release buffer.
        // That needs collision lookahead, so it is intentionally not faked here.
    }
};

class $modify(BufferPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = CCMenu::create();
        auto spr = ButtonSprite::create(stageLabel().c_str());
        spr->setScale(0.6f);
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [spr](CCMenuItemSpriteExtra*) {
            int64_t v = Mod::get()->getSettingValue<int64_t>("stage");
            v = v % STAGE_COUNT + 1;
            Mod::get()->setSettingValue<int64_t>("stage", v);
            spr->setString(stageLabel().c_str());
        });
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition({win.width / 2, 28.f});
        menu->setID("buffer-stage-menu"_spr);
        this->addChild(menu);
    }
};
