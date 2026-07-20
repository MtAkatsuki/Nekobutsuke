#pragma once
#include "CommonTypes.h"
#include "IniConfig.h"

// =========================================================
// Fx namespace
// 3D パーティクル演出の調整パラメータを一括管理する。
// ・エフェクトごとに struct でグループ化（フィールド名 = 意味）
// ・inline 変数のため ImGui から直接ポインタで調整可能
// ・調整が決まったらここの初期値を書き換えて固化する（プロジェクト慣例）
// =========================================================
namespace Fx {

    // --- 描画共通：寿命に対する見た目カーブ ---
    struct CurveParams {
        float fadeStartRatio = 0.3f;   // 残寿命比がこれを切ったらアルファフェード開始
        float shrinkStartRatio = 0.5f; // 残寿命比がこれを切ったら縮小開始
    };
    inline CurveParams Curve{};

    // --- 描画方式 ---
    struct RenderParams {
        int additive = 0;   // 1 = 加算合成（発光感が強いが、明るい背景では薄くなる）
    };
    inline RenderParams Render{};

    // --- 土塊（採掘・壁衝突）：放物線＋地面バウンド ---
    struct RubbleParams {
        int   count = 6;            // 一度に生成する個数
        float life = 0.8f;          // 寿命（秒）
        float gravity = 28.0f;      // 重力
        float upMin = 3.5f;         // 上向き初速（最小）
        float upMax = 6.5f;         // 上向き初速（最大）
        float sideMin = 0.5f;       // 水平拡散速度（最小）
        float sideMax = 2.2f;       // 水平拡散速度（最大）
        float scaleMin = 0.10f;     // ランダムスケール下限
        float scaleMax = 0.20f;     // ランダムスケール上限
        float flatten = 0.8f;       // Y方向の潰し率（土塊らしい扁平感）
        float spinMax = 8.0f;       // 自転速度の上限（rad/s、全軸）
        float spawnLift = 0.15f;    // 生成位置の足元からの浮かせ量
        float groundLift = 0.05f;   // バウンド面の浮かせ量（地面との z-fight 回避）
        float restitution = 0.45f;  // バウンド反発係数（Y速度の跳ね返り率）
        float friction = 0.7f;      // バウンド時の水平速度減衰
        Color colors[3] = {
            Color(0.55f, 0.42f, 0.28f, 1.0f),   // 土色
            Color(0.45f, 0.34f, 0.24f, 1.0f),   // 濃い土
            Color(0.63f, 0.52f, 0.36f, 1.0f) }; // 乾いた土
    };
    inline RubbleParams Rubble{};

    // --- 打撃スパーク：細長い破片の全方位放射（急速・超短命） ---
    struct HitParams {
        int   count = 10;           // スパーク本数
        float life = 0.25f;         // 寿命（急促さの要）
        float gravity = 10.0f;      // 弱重力（直線的に飛ばすため小さめ）
        float speedMin = 5.0f;      // 放射速度（最小）
        float speedMax = 9.0f;      // 放射速度（最大）
        float elevMin = -0.4f;      // 垂直方向比率の下限（負 = やや下向きも許可）
        float elevMax = 0.9f;       // 垂直方向比率の上限
        Vector3 sparkScale = Vector3(0.30f, 0.06f, 0.06f); // 細長比率（X軸が長手）
        Color colors[2] = {
            Color(1.0f, 1.0f, 1.0f, 1.0f),      // 白
            Color(1.0f, 0.92f, 0.55f, 1.0f) };  // 淡黄
    };
    inline HitParams Hit{};

    // --- 死亡バースト：多色の紙吹雪を大量に爆散 ---
    struct BurstParams {
        int   count = 30;           // 生成個数（華やかさの主因子）
        float life = 0.9f;
        float gravity = 20.0f;
        float speedMin = 3.0f;      // 放射速度（最小）
        float speedMax = 7.5f;      // 放射速度（最大）
        float upBias = 3.0f;        // 上向き速度の底上げ（0〜この値を加算）
        float upFactor = 0.4f;      // 放射速度に比例した追加の上向き成分
        float scaleMin = 0.08f;
        float scaleMax = 0.16f;
        float spinMax = 12.0f;      // 高速スピンで紙吹雪感を出す
        float spawnYOffset = 0.5f;  // 発生位置の足元からの高さ（体の中心あたり）
        Color colors[5] = {
            Color(1.0f, 0.85f, 0.25f, 1.0f),    // 黄
            Color(1.0f, 0.55f, 0.20f, 1.0f),    // 橙
            Color(1.0f, 0.40f, 0.70f, 1.0f),    // 桃
            Color(0.40f, 0.85f, 1.00f, 1.0f),   // 水色
            Color(1.0f, 1.0f, 1.0f, 1.0f) };    // 白
    };
    inline BurstParams Burst{};

    // --- 飛翔トレイル：無重力でゆっくり漂い縮んで消える残気 ---
    struct TrailParams {
        float life = 0.45f;
        float scale = 0.13f;        // 等方スケール
        float drift = 0.4f;         // 漂い速度の上限（無重力なので小さく）
        float spin = 2.0f;          // ゆっくり回転
        float interval = 0.05f;     // 生成間隔（秒）… Unit::UpdateDeathFly が参照
        Color color = Color(0.95f, 0.95f, 0.90f, 0.9f);
    };
    inline TrailParams Trail{};

    // --- 消滅の十字スター：物理なし・ビルボード・sin カーブで拡縮 ---
    struct StarParams {
        float life = 0.55f;
        float armLen = 1.7f;        // 腕の長さ（最大時）
        float armThick = 0.22f;     // 腕の太さ
        float progress = 0.6f;      // 出現タイミング（放物線周期比、0.5 = 頂点）
        Color color = Color(1.0f, 0.93f, 0.35f, 1.0f);
    };
    inline StarParams Star{};

    // --- 死亡震え（Enemy::DEATH_SHAKE が参照） ---
    struct DeathShakeParams {
        float duration = 0.45f;     // 震える時間（実時間）
        float amp = 0.06f;          // 振幅（蓄力震え 0.01 より大きめ）
    };
    inline DeathShakeParams DeathShake{};

    // =========================================================
// INI 永続化（テーブル駆動、Camera::CONFIG_TABLE と同方式）
// =========================================================
    inline const char* INI_FILE = "nekobutsuke_fx.ini";

    inline IniTable& ConfigTable() {
        static IniTable t = [] {
            IniTable tb;
            // --- 共通カーブ ---
            tb.Add("CURVE_FADE_START", &Curve.fadeStartRatio);
            tb.Add("CURVE_SHRINK_START", &Curve.shrinkStartRatio);
            // --- 土塊 ---
            tb.Add("RUBBLE_COUNT", &Rubble.count);
            tb.Add("RUBBLE_LIFE", &Rubble.life);
            tb.Add("RUBBLE_GRAVITY", &Rubble.gravity);
            tb.Add("RUBBLE_UP_MIN", &Rubble.upMin);
            tb.Add("RUBBLE_UP_MAX", &Rubble.upMax);
            tb.Add("RUBBLE_SIDE_MIN", &Rubble.sideMin);
            tb.Add("RUBBLE_SIDE_MAX", &Rubble.sideMax);
            tb.Add("RUBBLE_SCALE_MIN", &Rubble.scaleMin);
            tb.Add("RUBBLE_SCALE_MAX", &Rubble.scaleMax);
            tb.Add("RUBBLE_FLATTEN", &Rubble.flatten);
            tb.Add("RUBBLE_SPIN_MAX", &Rubble.spinMax);
            tb.Add("RUBBLE_SPAWN_LIFT", &Rubble.spawnLift);
            tb.Add("RUBBLE_GROUND_LIFT", &Rubble.groundLift);
            tb.Add("RUBBLE_RESTITUTION", &Rubble.restitution);
            tb.Add("RUBBLE_FRICTION", &Rubble.friction);
            tb.Add("RUBBLE_COLOR_1", &Rubble.colors[0]);
            tb.Add("RUBBLE_COLOR_2", &Rubble.colors[1]);
            tb.Add("RUBBLE_COLOR_3", &Rubble.colors[2]);
            // --- 打撃スパーク ---
            tb.Add("HIT_COUNT", &Hit.count);
            tb.Add("HIT_LIFE", &Hit.life);
            tb.Add("HIT_GRAVITY", &Hit.gravity);
            tb.Add("HIT_SPEED_MIN", &Hit.speedMin);
            tb.Add("HIT_SPEED_MAX", &Hit.speedMax);
            tb.Add("HIT_ELEV_MIN", &Hit.elevMin);
            tb.Add("HIT_ELEV_MAX", &Hit.elevMax);
            tb.Add("HIT_SPARK_SCALE", &Hit.sparkScale);
            tb.Add("HIT_COLOR_1", &Hit.colors[0]);
            tb.Add("HIT_COLOR_2", &Hit.colors[1]);
            // --- 死亡バースト ---
            tb.Add("BURST_COUNT", &Burst.count);
            tb.Add("BURST_LIFE", &Burst.life);
            tb.Add("BURST_GRAVITY", &Burst.gravity);
            tb.Add("BURST_SPEED_MIN", &Burst.speedMin);
            tb.Add("BURST_SPEED_MAX", &Burst.speedMax);
            tb.Add("BURST_UP_BIAS", &Burst.upBias);
            tb.Add("BURST_UP_FACTOR", &Burst.upFactor);
            tb.Add("BURST_SCALE_MIN", &Burst.scaleMin);
            tb.Add("BURST_SCALE_MAX", &Burst.scaleMax);
            tb.Add("BURST_SPIN_MAX", &Burst.spinMax);
            tb.Add("BURST_Y_OFFSET", &Burst.spawnYOffset);
            tb.Add("BURST_COLOR_1", &Burst.colors[0]);
            tb.Add("BURST_COLOR_2", &Burst.colors[1]);
            tb.Add("BURST_COLOR_3", &Burst.colors[2]);
            tb.Add("BURST_COLOR_4", &Burst.colors[3]);
            tb.Add("BURST_COLOR_5", &Burst.colors[4]);
            // --- トレイル ---
            tb.Add("TRAIL_LIFE", &Trail.life);
            tb.Add("TRAIL_SCALE", &Trail.scale);
            tb.Add("TRAIL_DRIFT", &Trail.drift);
            tb.Add("TRAIL_SPIN", &Trail.spin);
            tb.Add("TRAIL_INTERVAL", &Trail.interval);
            tb.Add("TRAIL_COLOR", &Trail.color);
            // --- 十字スター ---
            tb.Add("STAR_LIFE", &Star.life);
            tb.Add("STAR_ARM_LEN", &Star.armLen);
            tb.Add("STAR_ARM_THICK", &Star.armThick);
            tb.Add("STAR_PROGRESS", &Star.progress);
            tb.Add("STAR_COLOR", &Star.color);
            // --- 死亡震え ---
            tb.Add("SHAKE_DURATION", &DeathShake.duration);
            tb.Add("SHAKE_AMP", &DeathShake.amp);
            return tb;
            }();
        return t;
    }

    inline bool LoadConfig() { return ConfigTable().Load(INI_FILE); }
    inline bool SaveConfig() { return ConfigTable().Save(INI_FILE); }
}