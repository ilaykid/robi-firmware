#ifndef ROBI_FACE_MATH_H_
#define ROBI_FACE_MATH_H_

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace robi {

enum RobiFaceMode {
    ROBI_MODE_IDLE = 0,
    ROBI_MODE_LISTENING = 1,
    ROBI_MODE_THINKING = 2,
    ROBI_MODE_SPEAKING = 3,
};

enum RobiThemeId {
    ROBI_THEME_CYBER_CYAN = 0,
    ROBI_THEME_SOLAR_GOLD = 1,
    ROBI_THEME_EMERALD_MATRIX = 2,
    ROBI_THEME_COSMIC_VIOLET = 3,
    ROBI_THEME_CORAL_CUTE = 4,
};

enum RobiFaceStyle {
    ROBI_STYLE_CYBER_VISOR = 0,
    ROBI_STYLE_ROUND_KAWAII = 1,
    ROBI_STYLE_RETRO_PIXEL = 2,
    ROBI_STYLE_VISOR_CYCLOPS = 3,
    ROBI_STYLE_OWL_MECH = 4,
};

struct FaceParams {
    float eye_w = 92.0f;
    float eye_h = 98.0f;
    float eye_radius = 32.0f;
    float pupil_scale = 0.34f;
    float gaze_x = 0.0f;
    float gaze_y = 0.0f;
    float squint_top_l = 0.0f;
    float squint_top_r = 0.0f;
    float squint_bot_l = 0.0f;
    float squint_bot_r = 0.0f;
    float brow_ang_l = 0.0f;
    float brow_ang_r = 0.0f;
    float brow_y_l = 0.0f;
    float brow_y_r = 0.0f;
    float brow_opa = 0.78f;
    float mouth_w = 58.0f;
    float mouth_curve = 12.0f;
    float mouth_tilt = 0.0f;
    float mouth_wave = 0.0f;
    float mouth_o = 0.0f;
    float cheek_opa = 0.0f;
    float visor_opa = 0.0f;
    float col_r = 0.0f;
    float col_g = 229.0f;
    float col_b = 255.0f;
};

inline float RobiClamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float RobiLerp(float a, float b, float k) {
    return a + (b - a) * k;
}

inline uint32_t RobiPackRgb(float r, float g, float b) {
    int ri = static_cast<int>(RobiClamp(r, 0.0f, 255.0f));
    int gi = static_cast<int>(RobiClamp(g, 0.0f, 255.0f));
    int bi = static_cast<int>(RobiClamp(b, 0.0f, 255.0f));
    return (static_cast<uint32_t>(ri) << 16) | (static_cast<uint32_t>(gi) << 8) |
           static_cast<uint32_t>(bi);
}

inline RobiThemeId RobiParseThemeId(std::string_view t) {
    if (t == "solar_gold") return ROBI_THEME_SOLAR_GOLD;
    if (t == "emerald_matrix") return ROBI_THEME_EMERALD_MATRIX;
    if (t == "cosmic_violet") return ROBI_THEME_COSMIC_VIOLET;
    if (t == "coral_cute") return ROBI_THEME_CORAL_CUTE;
    return ROBI_THEME_CYBER_CYAN;
}

inline RobiFaceStyle RobiParseFaceStyle(std::string_view s) {
    if (s == "round_kawaii") return ROBI_STYLE_ROUND_KAWAII;
    if (s == "retro_pixel") return ROBI_STYLE_RETRO_PIXEL;
    if (s == "visor_cyclops") return ROBI_STYLE_VISOR_CYCLOPS;
    if (s == "owl_mech") return ROBI_STYLE_OWL_MECH;
    return ROBI_STYLE_CYBER_VISOR;
}

inline uint32_t RobiThemeBaseHex(RobiThemeId theme_id) {
    switch (theme_id) {
        case ROBI_THEME_SOLAR_GOLD: return 0xFFB703;
        case ROBI_THEME_EMERALD_MATRIX: return 0x10B981;
        case ROBI_THEME_COSMIC_VIOLET: return 0xA855F7;
        case ROBI_THEME_CORAL_CUTE: return 0xFF5C8A;
        case ROBI_THEME_CYBER_CYAN:
        default: return 0x00E5FF;
    }
}

inline FaceParams RobiDefaultParams(RobiThemeId theme_id, RobiFaceStyle style_id) {
    FaceParams p = {};
    p.eye_w = 92.0f;
    p.eye_h = 98.0f;
    p.eye_radius = 32.0f;
    p.pupil_scale = 0.34f;
    p.brow_opa = 0.78f;
    p.mouth_w = 58.0f;
    p.mouth_curve = 12.0f;

    if (style_id == ROBI_STYLE_ROUND_KAWAII) {
        p.eye_w = 106.0f;
        p.eye_h = 106.0f;
        p.eye_radius = 53.0f;
        p.pupil_scale = 0.54f;
        p.brow_opa = 0.58f;
        p.brow_y_l = -5.0f;
        p.brow_y_r = -5.0f;
        p.mouth_w = 50.0f;
        p.mouth_curve = 18.0f;
        p.cheek_opa = 0.72f;
    } else if (style_id == ROBI_STYLE_RETRO_PIXEL) {
        p.eye_w = 98.0f;
        p.eye_h = 86.0f;
        p.eye_radius = 6.0f;
        p.pupil_scale = 0.36f;
        p.brow_opa = 0.90f;
        p.mouth_w = 58.0f;
        p.mouth_curve = 5.0f;
    } else if (style_id == ROBI_STYLE_VISOR_CYCLOPS) {
        p.eye_w = 114.0f;
        p.eye_h = 50.0f;
        p.eye_radius = 16.0f;
        p.pupil_scale = 0.30f;
        p.brow_opa = 0.80f;
        p.brow_y_l = -4.0f;
        p.brow_y_r = -4.0f;
        p.mouth_w = 66.0f;
        p.mouth_curve = 5.0f;
        p.visor_opa = 0.92f;
    } else if (style_id == ROBI_STYLE_OWL_MECH) {
        p.eye_w = 102.0f;
        p.eye_h = 102.0f;
        p.eye_radius = 51.0f;
        p.pupil_scale = 0.28f;
        p.brow_opa = 0.92f;
        p.brow_ang_l = -3.0f;
        p.brow_ang_r = -3.0f;
        p.mouth_w = 54.0f;
        p.mouth_curve = 8.0f;
    }

    uint32_t hex = RobiThemeBaseHex(theme_id);
    p.col_r = static_cast<float>((hex >> 16) & 0xFF);
    p.col_g = static_cast<float>((hex >> 8) & 0xFF);
    p.col_b = static_cast<float>(hex & 0xFF);
    return p;
}

struct EmotionPresetResult {
    FaceParams params;
    bool bounce_laugh = false;
    bool tremor_shock = false;
};

inline const char* RobiNormalizeEmotionName(const char* name) {
    if (name == nullptr || name[0] == '\0') return "neutral";
    if (std::strcmp(name, "wink") == 0) return "winking";
    if (std::strcmp(name, "love") == 0 || std::strcmp(name, "affectionate") == 0) return "loving";
    if (std::strcmp(name, "laugh") == 0 || std::strcmp(name, "amused") == 0) return "laughing";
    if (std::strcmp(name, "excited") == 0 || std::strcmp(name, "amazed") == 0) return "surprised";
    if (std::strcmp(name, "proud") == 0) return "confident";
    if (std::strcmp(name, "curious") == 0) return "thinking";
    if (std::strcmp(name, "playful") == 0) return "silly";
    if (std::strcmp(name, "tired") == 0) return "sleepy";
    return name;
}

// Computes procedural target FaceParams for any emotion while strictly preserving
// the user's selected theme color (`RobiThemeBaseHex(theme_id)`).
inline EmotionPresetResult RobiComputePresetParams(const char* raw_name, RobiThemeId theme_id,
                                                   RobiFaceStyle style_id) {
    EmotionPresetResult res;
    res.params = RobiDefaultParams(theme_id, style_id);
    if (raw_name == nullptr) return res;
    const char* name = RobiNormalizeEmotionName(raw_name);

    FaceParams& tgt = res.params;
    if (std::strcmp(name, "listening") == 0) {
        tgt.eye_w += 6.0f;
        tgt.eye_h += 8.0f;
        tgt.pupil_scale += 0.05f;
        tgt.gaze_y = -2.0f;
        tgt.brow_y_l = -5.0f;
        tgt.brow_y_r = -5.0f;
        tgt.brow_opa = 0.85f;
        tgt.mouth_w = 36.0f;
        tgt.mouth_curve = 9.0f;
    } else if (std::strcmp(name, "thinking") == 0) {
        tgt.eye_w -= 2.0f;
        tgt.eye_h -= 4.0f;
        tgt.gaze_x = 15.0f;
        tgt.gaze_y = -11.0f;
        tgt.squint_bot_l = 0.20f;
        tgt.squint_top_r = 0.08f;
        tgt.brow_ang_l = 7.0f;
        tgt.brow_ang_r = 12.0f;
        tgt.brow_y_l = 2.0f;
        tgt.brow_y_r = -9.0f;
        tgt.brow_opa = 0.95f;
        tgt.mouth_w = 32.0f;
        tgt.mouth_curve = -2.0f;
        tgt.mouth_tilt = 6.0f;
    } else if (std::strcmp(name, "happy") == 0) {
        tgt.eye_w += 4.0f;
        tgt.eye_h += 2.0f;
        tgt.squint_bot_l = 0.50f;
        tgt.squint_bot_r = 0.50f;
        tgt.brow_y_l = -5.0f;
        tgt.brow_y_r = -5.0f;
        tgt.brow_opa = 0.75f;
        tgt.mouth_w = 50.0f;
        tgt.mouth_curve = 24.0f;
        tgt.cheek_opa = std::max(tgt.cheek_opa, 0.34f);
    } else if (std::strcmp(name, "laughing") == 0 || std::strcmp(name, "funny") == 0) {
        res.bounce_laugh = true;
        tgt.eye_w += 8.0f;
        tgt.squint_bot_l = 0.68f;
        tgt.squint_bot_r = 0.68f;
        tgt.brow_y_l = -7.0f;
        tgt.brow_y_r = -7.0f;
        tgt.brow_opa = 0.88f;
        tgt.mouth_w = 58.0f;
        tgt.mouth_curve = 32.0f;
        tgt.cheek_opa = std::max(tgt.cheek_opa, 0.55f);
    } else if (std::strcmp(name, "loving") == 0) {
        tgt.eye_w += 6.0f;
        tgt.eye_h += 6.0f;
        tgt.pupil_scale = 0.34f;
        tgt.squint_bot_l = 0.46f;
        tgt.squint_bot_r = 0.46f;
        tgt.brow_ang_l = -6.0f;
        tgt.brow_ang_r = 6.0f;
        tgt.brow_y_l = -4.0f;
        tgt.brow_y_r = -4.0f;
        tgt.brow_opa = 0.80f;
        tgt.mouth_w = 48.0f;
        tgt.mouth_curve = 25.0f;
        tgt.cheek_opa = 0.88f;
    } else if (std::strcmp(name, "kissy") == 0) {
        tgt.squint_bot_l = 0.20f;
        tgt.squint_bot_r = 0.74f;
        tgt.squint_top_r = 0.18f;
        tgt.brow_y_l = -6.0f;
        tgt.brow_opa = 0.78f;
        tgt.mouth_w = 18.0f;
        tgt.mouth_o = 0.88f;
        tgt.cheek_opa = 0.80f;
    } else if (std::strcmp(name, "surprised") == 0) {
        tgt.eye_w = 116.0f;
        tgt.eye_h = 116.0f;
        tgt.eye_radius = 58.0f;
        tgt.pupil_scale = 0.20f;
        tgt.brow_y_l = -14.0f;
        tgt.brow_y_r = -14.0f;
        tgt.brow_opa = 1.0f;
        tgt.mouth_w = 26.0f;
        tgt.mouth_o = 0.92f;
    } else if (std::strcmp(name, "shocked") == 0) {
        res.tremor_shock = true;
        tgt.eye_w = 120.0f;
        tgt.eye_h = 120.0f;
        tgt.eye_radius = 60.0f;
        tgt.pupil_scale = 0.16f;
        tgt.brow_y_l = -18.0f;
        tgt.brow_y_r = -18.0f;
        tgt.brow_opa = 1.0f;
        tgt.mouth_w = 34.0f;
        tgt.mouth_o = 1.0f;
    } else if (std::strcmp(name, "winking") == 0) {
        tgt.squint_bot_r = 0.76f;
        tgt.squint_top_r = 0.20f;
        tgt.brow_y_l = -8.0f;
        tgt.brow_y_r = 3.0f;
        tgt.brow_opa = 0.90f;
        tgt.mouth_w = 46.0f;
        tgt.mouth_curve = 18.0f;
        tgt.mouth_tilt = 11.0f;
        tgt.cheek_opa = std::max(tgt.cheek_opa, 0.35f);
    } else if (std::strcmp(name, "cool") == 0) {
        tgt.squint_top_l = 0.22f;
        tgt.squint_top_r = 0.22f;
        tgt.brow_ang_l = 6.0f;
        tgt.brow_ang_r = -6.0f;
        tgt.brow_opa = 0.85f;
        tgt.visor_opa = 0.92f;
        tgt.mouth_w = 46.0f;
        tgt.mouth_curve = 14.0f;
        tgt.mouth_tilt = 10.0f;
    } else if (std::strcmp(name, "confident") == 0) {
        tgt.squint_top_l = 0.20f;
        tgt.squint_top_r = 0.20f;
        tgt.squint_bot_l = 0.18f;
        tgt.squint_bot_r = 0.18f;
        tgt.brow_ang_l = 9.0f;
        tgt.brow_ang_r = -4.0f;
        tgt.brow_y_r = -6.0f;
        tgt.brow_opa = 0.95f;
        tgt.mouth_w = 48.0f;
        tgt.mouth_curve = 16.0f;
        tgt.mouth_tilt = 10.0f;
    } else if (std::strcmp(name, "confused") == 0) {
        tgt.gaze_x = -9.0f;
        tgt.gaze_y = -4.0f;
        tgt.squint_top_l = 0.32f;
        tgt.brow_ang_l = 12.0f;
        tgt.brow_ang_r = 10.0f;
        tgt.brow_y_l = 4.0f;
        tgt.brow_y_r = -10.0f;
        tgt.brow_opa = 1.0f;
        tgt.mouth_w = 42.0f;
        tgt.mouth_curve = -3.0f;
        tgt.mouth_wave = 6.0f;
    } else if (std::strcmp(name, "silly") == 0) {
        res.bounce_laugh = true;
        tgt.gaze_x = 8.0f;
        tgt.squint_bot_l = 0.56f;
        tgt.squint_bot_r = 0.12f;
        tgt.brow_y_l = 2.0f;
        tgt.brow_y_r = -10.0f;
        tgt.brow_opa = 0.90f;
        tgt.mouth_w = 52.0f;
        tgt.mouth_curve = 22.0f;
        tgt.mouth_wave = 4.5f;
        tgt.cheek_opa = 0.40f;
    } else if (std::strcmp(name, "embarrassed") == 0) {
        tgt.gaze_x = -12.0f;
        tgt.gaze_y = 9.0f;
        tgt.squint_bot_l = 0.28f;
        tgt.squint_bot_r = 0.28f;
        tgt.brow_ang_l = -10.0f;
        tgt.brow_ang_r = 10.0f;
        tgt.brow_opa = 0.90f;
        tgt.mouth_w = 30.0f;
        tgt.mouth_wave = 4.0f;
        tgt.cheek_opa = 0.92f;
    } else if (std::strcmp(name, "relaxed") == 0 || std::strcmp(name, "delicious") == 0) {
        tgt.squint_bot_l = 0.60f;
        tgt.squint_bot_r = 0.60f;
        tgt.squint_top_l = 0.14f;
        tgt.squint_top_r = 0.14f;
        tgt.brow_opa = 0.55f;
        tgt.mouth_w = 44.0f;
        tgt.mouth_curve = 20.0f;
        tgt.cheek_opa = 0.35f;
    } else if (std::strcmp(name, "sad") == 0) {
        tgt.gaze_y = 11.0f;
        tgt.squint_top_l = 0.34f;
        tgt.squint_top_r = 0.34f;
        tgt.brow_ang_l = -14.0f;
        tgt.brow_ang_r = 14.0f;
        tgt.brow_y_l = -2.0f;
        tgt.brow_y_r = -2.0f;
        tgt.brow_opa = 1.0f;
        tgt.mouth_w = 38.0f;
        tgt.mouth_curve = -15.0f;
    } else if (std::strcmp(name, "crying") == 0) {
        tgt.gaze_y = 10.0f;
        tgt.squint_top_l = 0.28f;
        tgt.squint_top_r = 0.28f;
        tgt.squint_bot_l = 0.42f;
        tgt.squint_bot_r = 0.42f;
        tgt.brow_ang_l = -16.0f;
        tgt.brow_ang_r = 16.0f;
        tgt.brow_opa = 1.0f;
        tgt.mouth_w = 40.0f;
        tgt.mouth_curve = -20.0f;
        tgt.mouth_wave = 2.5f;
    } else if (std::strcmp(name, "angry") == 0) {
        tgt.squint_top_l = 0.34f;
        tgt.squint_top_r = 0.34f;
        tgt.squint_bot_l = 0.16f;
        tgt.squint_bot_r = 0.16f;
        tgt.brow_ang_l = 18.0f;
        tgt.brow_ang_r = -18.0f;
        tgt.brow_y_l = 5.0f;
        tgt.brow_y_r = 5.0f;
        tgt.brow_opa = 1.0f;
        tgt.mouth_w = 40.0f;
        tgt.mouth_curve = -12.0f;
    } else if (std::strcmp(name, "sleepy") == 0) {
        tgt.gaze_y = 6.0f;
        tgt.squint_top_l = 0.72f;
        tgt.squint_top_r = 0.72f;
        tgt.brow_opa = 0.40f;
        tgt.mouth_w = 16.0f;
        tgt.mouth_o = 0.55f;
    } else if (std::strcmp(name, "dizzy") == 0) {
        res.tremor_shock = true;
        tgt.eye_w = 108.0f;
        tgt.eye_h = 108.0f;
        tgt.eye_radius = 54.0f;
        tgt.pupil_scale = 0.22f;
        tgt.squint_top_l = 0.26f;
        tgt.squint_bot_r = 0.26f;
        tgt.brow_ang_l = 16.0f;
        tgt.brow_ang_r = 16.0f;
        tgt.brow_y_l = -10.0f;
        tgt.brow_y_r = 4.0f;
        tgt.brow_opa = 1.0f;
        tgt.mouth_w = 48.0f;
        tgt.mouth_curve = 0.0f;
        tgt.mouth_wave = 9.5f;
        tgt.cheek_opa = 0.45f;
    }
    return res;
}

}  // namespace robi

#endif  // ROBI_FACE_MATH_H_
