#ifndef ROBI_GAMES_H_
#define ROBI_GAMES_H_

#include "robi_utf8.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace robi {

struct WordCategoryDef {
    const char* id;
    const char* title_he;
    const char* title_en;
    uint32_t color_hex;
    const char* const* words_he;
    size_t count_he;
    const char* const* words_en;
    size_t count_en;
};

namespace detail {

inline constexpr const char* kAnimalsHe[] = {
    "פיל",        "אריה",      "ג'ירפה",     "פינגווין",   "דולפין",    "קנגורו",
    "תמנון",      "זיקית",     "עצלן",       "כריש",       "תוכי",      "ארנב",
    "קיפוד",      "טווס",      "דוב קוטב",   "סנאי",       "צפרדע",     "נמר",
    "גמל",        "ינשוף",     "פרפר",       "תנין",       "פלמינגו",   "כלב ים",
    "דבורה",      "היפופוטם",  "זברה",       "קואלה",      "סוס",       "עטלף",
    "לווייתן",    "צב ים",     "קוף",        "זאב",        "שועל",      "תרנגול",
    "מדוזה",      "סרטן",      "נשר",        "פנדה",
};
inline constexpr const char* kAnimalsEn[] = {
    "Elephant",   "Lion",      "Giraffe",    "Penguin",    "Dolphin",   "Kangaroo",
    "Octopus",    "Chameleon", "Sloth",      "Shark",      "Parrot",    "Rabbit",
    "Hedgehog",   "Peacock",   "Polar Bear", "Squirrel",   "Frog",      "Tiger",
    "Camel",      "Owl",       "Butterfly",  "Crocodile",  "Flamingo",  "Seal",
    "Bumblebee",  "Hippo",     "Zebra",      "Koala",      "Horse",     "Panda",
};

inline constexpr const char* kFoodHe[] = {
    "פיצה",       "גלידה",     "חלווה",      "שוקולד",     "פלאפל",     "המבורגר",
    "פופקורן",    "אבטיח",     "ספגטי",      "פנקייק",     "צ'יפס",     "סושי",
    "שניצל",      "סופגנייה",  "במבה",       "תירס חם",    "עוגה",      "מלפפון חמוץ",
    "קרמבו",      "מנגו",      "קורנפלקס",   "שקשוקה",     "ביצה עין",  "צמר גפן מתוק",
    "בייגלה",     "תות שדה",   "אננס",       "פיתה",       "חומוס",     "בורקס",
    "וופל בלגי",  "שייק פירות","ענבים",      "בננה",       "מרק עוף",   "טוסט גבינה",
    "אוזן המן",   "תפוח בדבש", "לזניה",      "אבוקדו",
};
inline constexpr const char* kFoodEn[] = {
    "Pizza",      "Ice Cream", "Halva",      "Chocolate",  "Falafel",   "Hamburger",
    "Popcorn",    "Watermelon","Spaghetti",  "Pancakes",   "French Fries","Sushi",
    "Donut",      "Hot Corn",  "Birthday Cake","Pickle",   "Mango",     "Waffles",
    "Cotton Candy","Pretzel",  "Strawberry", "Pineapple",  "Smoothie",  "Banana",
    "Avocado",    "Taco",      "Cheese Toast","Lasagna",   "Apple Pie", "Cookie",
};

inline constexpr const char* kSportsActionsHe[] = {
    "כדורגל",         "כדורסל",         "שחייה",          "קפיצה בחבל",
    "רכיבה על אופניים","צחצוח שיניים",   "ניגון בגיטרה",   "ריקוד",
    "גלישת גלים",     "טיפוס הרים",     "קראטה",          "טניס",
    "סקייטבורד",      "החלקה על הקרח",  "צילום סלפי",     "בישול בסיר",
    "נהיגה במרוץ",    "זריקה לסל",      "שירה במיקרופון", "יוגה",
    "כדורעף חופים",   "באולינג",        "תיפוף בתופים",   "קפיצה בטרמפולינה",
    "הטסת עפיפון",    "בניית ארמון חול","דיג בחכה",       "ריצת מרתון",
    "קסם עם קלפים",   "התעמלות קרקע",   "פינג פונג",      "קליעה למטרה",
    "ניפוח בלון",     "ג'אגלינג",       "הליכה על חבל",   "שוער בכדורגל",
};
inline constexpr const char* kSportsActionsEn[] = {
    "Soccer",         "Basketball",     "Swimming",       "Jump Rope",
    "Riding a Bike",  "Brushing Teeth", "Playing Guitar", "Breakdancing",
    "Surfing",        "Rock Climbing",  "Karate",         "Tennis",
    "Skateboarding",  "Ice Skating",    "Taking a Selfie","Cooking",
    "Race Car Driving","Singing",       "Bowling",        "Playing Drums",
    "Trampoline",     "Flying a Kite",  "Fishing",        "Juggling",
};

inline constexpr const char* kCharactersHe[] = {
    "אסטרונאוט",  "גיבור על",  "פיראט",      "דינוזאור",   "קוסם",      "נינג'ה",
    "רובוט",      "בלש",       "דרקון",      "כבאי",       "חייזר",     "אביר",
    "בת הים",     "טייס",      "מדען",       "ליצן",       "שוטר",      "רופא",
    "שף",         "מלך",       "טרזן",       "קאובוי",     "ממציא",     "צייר",
    "צלם",        "קפטן ספינה","חוקר טבע",   "שחקן קולנוע","זמר רוק",   "אצן אולימפי",
    "מאלף אריות", "מציל בים",  "נהג קטר",    "גנן",        "קוסמונאוט", "סמוראי",
};
inline constexpr const char* kCharactersEn[] = {
    "Astronaut",  "Superhero", "Pirate",     "Dinosaur",   "Wizard",    "Ninja",
    "Robot",      "Detective", "Dragon",     "Firefighter","Alien",     "Knight",
    "Mermaid",    "Pilot",     "Scientist",  "Clown",      "Chef",      "Cowboy",
    "Inventor",   "Rock Star", "Samurai",    "Lifeguard",  "Explorer",  "King",
};

inline constexpr const char* kObjectsPlacesHe[] = {
    "מטוס",       "רכבת הרים", "טרמפולינה",  "מטרייה",     "משקפי שמש", "שעון מעורר",
    "טלסקופ",     "אוהל",      "צוללת",      "מכונת כביסה","פנס",       "תיק גב",
    "כדור פורח",  "מגלשת מים", "קולנוע",     "חוף הים",    "ספרייה",    "גן חיות",
    "מצלמה",      "אוזניות",   "מסוק",       "מגדלור",     "טירה",      "אי בודד",
    "הר געש",     "חללית",     "קורקינט",    "מיקרוסקופ",  "מצפן",      "תיבת אוצר",
    "בועות סבון", "שלט רחוק",  "מאוורר",     "מזוודה",     "מראה",      "כתר זהב",
};
inline constexpr const char* kObjectsPlacesEn[] = {
    "Airplane",   "Roller Coaster","Trampoline","Umbrella","Sunglasses","Alarm Clock",
    "Telescope",  "Tent",      "Submarine",  "Flashlight", "Hot Air Balloon","Water Slide",
    "Movie Theater","Beach",   "Zoo",        "Camera",     "Helicopter","Volcano",
    "Spaceship",  "Scooter",   "Compass",    "Treasure Chest","Soap Bubbles","Crown",
};

}  // namespace detail

inline constexpr WordCategoryDef kHeadsUpCategories[5] = {
    {"animals", "חיות", "Animals", 0x10B981,
     detail::kAnimalsHe, sizeof(detail::kAnimalsHe) / sizeof(detail::kAnimalsHe[0]),
     detail::kAnimalsEn, sizeof(detail::kAnimalsEn) / sizeof(detail::kAnimalsEn[0])},
    {"food", "אוכל וממתקים", "Food & Treats", 0xF59E0B,
     detail::kFoodHe, sizeof(detail::kFoodHe) / sizeof(detail::kFoodHe[0]),
     detail::kFoodEn, sizeof(detail::kFoodEn) / sizeof(detail::kFoodEn[0])},
    {"actions", "ספורט ופעולות", "Sports & Actions", 0x00E5FF,
     detail::kSportsActionsHe, sizeof(detail::kSportsActionsHe) / sizeof(detail::kSportsActionsHe[0]),
     detail::kSportsActionsEn, sizeof(detail::kSportsActionsEn) / sizeof(detail::kSportsActionsEn[0])},
    {"chars", "דמויות", "Characters", 0xA855F7,
     detail::kCharactersHe, sizeof(detail::kCharactersHe) / sizeof(detail::kCharactersHe[0]),
     detail::kCharactersEn, sizeof(detail::kCharactersEn) / sizeof(detail::kCharactersEn[0])},
    {"objects", "חפצים ומקומות", "Objects & Places", 0xFF5C8A,
     detail::kObjectsPlacesHe, sizeof(detail::kObjectsPlacesHe) / sizeof(detail::kObjectsPlacesHe[0]),
     detail::kObjectsPlacesEn, sizeof(detail::kObjectsPlacesEn) / sizeof(detail::kObjectsPlacesEn[0])},
};

// Pure state machine for "נחש מה על המצח" (Heads Up! Forehead Charades)
class HeadsUpEngine {
public:
    enum class Phase : uint8_t {
        kInactive = 0,
        kCountdown = 1,
        kPlaying = 2,
        kFlashCorrect = 3,
        kFlashSkip = 4,
        kSummary = 5,
    };

    struct TickEvents {
        bool countdown_tick = false;
        bool round_started = false;
        bool flash_ended = false;
        bool round_finished = false;
    };

    void SeedRng(uint32_t seed) {
        rng_state_ = (seed == 0) ? 0x9E3779B9u : seed;
    }

    // cat_index: 0..4 for specific category, or -1 for shuffled Mega-Mix across all categories
    void StartOfflineRound(int cat_index, bool english, int round_seconds = 60) {
        std::vector<std::string> pool;
        if (cat_index >= 0 && cat_index < 5) {
            const auto& c = kHeadsUpCategories[cat_index];
            topic_title_ = english ? c.title_en : c.title_he;
            const char* const* arr = english ? c.words_en : c.words_he;
            size_t n = english ? c.count_en : c.count_he;
            pool.reserve(n);
            for (size_t i = 0; i < n; ++i) pool.emplace_back(arr[i]);
        } else {
            topic_title_ = english ? "Mega Mix" : "ערבוב משפחתי";
            for (const auto& c : kHeadsUpCategories) {
                const char* const* arr = english ? c.words_en : c.words_he;
                size_t n = english ? c.count_en : c.count_he;
                for (size_t i = 0; i < n; ++i) pool.emplace_back(arr[i]);
            }
        }
        ShufflePool(pool);
        BeginWithDeck(std::move(pool), round_seconds);
    }

    void StartCustomRound(const std::string& topic, const std::vector<std::string>& custom_words,
                          bool english, int round_seconds = 60) {
        std::vector<std::string> pool;
        for (const auto& w : custom_words) {
            std::string clean = Utf8SafeTruncate(w, 42);
            if (!clean.empty()) {
                pool.push_back(std::move(clean));
            }
        }
        if (pool.size() < 3) {
            StartOfflineRound(-1, english, round_seconds);
            return;
        }
        topic_title_ = topic.empty() ? (english ? "AI Custom" : "נושא אישי")
                                     : Utf8SafeTruncate(topic, 36);
        ShufflePool(pool);
        BeginWithDeck(std::move(pool), round_seconds);
    }

    void Stop() {
        phase_ = Phase::kInactive;
        deck_.clear();
        guessed_words_.clear();
    }

    bool IsActive() const { return phase_ != Phase::kInactive; }
    bool IsForeheadSensorNeeded() const {
        return phase_ == Phase::kCountdown || phase_ == Phase::kPlaying ||
               phase_ == Phase::kFlashCorrect || phase_ == Phase::kFlashSkip;
    }

    Phase phase() const { return phase_; }
    const std::string& topic_title() const { return topic_title_; }
    int countdown_sec() const { return (countdown_ms_ + 999) / 1000; }
    int remaining_sec() const { return (remaining_ms_ + 999) / 1000; }
    int correct_count() const { return correct_count_; }
    int skipped_count() const { return skipped_count_; }
    const std::vector<std::string>& guessed_words() const { return guessed_words_; }

    std::string CurrentWord() const {
        if (deck_.empty()) return "";
        return deck_[deck_idx_ % deck_.size()];
    }

    bool MarkCorrect() {
        if (phase_ != Phase::kPlaying) return false;
        if (!deck_.empty()) {
            guessed_words_.push_back(CurrentWord());
            ++deck_idx_;
        }
        ++correct_count_;
        if (deck_idx_ >= deck_.size()) {
            phase_ = Phase::kSummary;
            return true;
        }
        phase_ = Phase::kFlashCorrect;
        flash_ms_ = 850;
        return true;
    }

    bool MarkSkip() {
        if (phase_ != Phase::kPlaying) return false;
        if (!deck_.empty()) {
            ++deck_idx_;
        }
        ++skipped_count_;
        if (deck_idx_ >= deck_.size()) {
            phase_ = Phase::kSummary;
            return true;
        }
        phase_ = Phase::kFlashSkip;
        flash_ms_ = 750;
        return true;
    }

    TickEvents Step(int dt_ms) {
        TickEvents ev = {};
        if (dt_ms <= 0 || phase_ == Phase::kInactive || phase_ == Phase::kSummary) {
            return ev;
        }

        if (phase_ == Phase::kCountdown) {
            int prev_sec = countdown_sec();
            countdown_ms_ = std::max(0, countdown_ms_ - dt_ms);
            int cur_sec = countdown_sec();
            if (countdown_ms_ == 0) {
                phase_ = Phase::kPlaying;
                ev.round_started = true;
            } else if (cur_sec != prev_sec) {
                ev.countdown_tick = true;
            }
            return ev;
        }

        // Round timer continues ticking during brief Correct/Skip flash overlays
        int prev_rem_sec = remaining_sec();
        remaining_ms_ = std::max(0, remaining_ms_ - dt_ms);
        int cur_rem_sec = remaining_sec();
        if (remaining_ms_ == 0) {
            phase_ = Phase::kSummary;
            ev.round_finished = true;
            return ev;
        }
        if (cur_rem_sec != prev_rem_sec && cur_rem_sec >= 1 && cur_rem_sec <= 5) {
            ev.countdown_tick = true;
        }

        if (phase_ == Phase::kFlashCorrect || phase_ == Phase::kFlashSkip) {
            flash_ms_ = std::max(0, flash_ms_ - dt_ms);
            if (flash_ms_ == 0) {
                phase_ = Phase::kPlaying;
                ev.flash_ended = true;
            }
        }
        return ev;
    }

private:
    uint32_t NextRand() {
        uint32_t x = rng_state_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rng_state_ = (x == 0) ? 0x6D2B79F5u : x;
        return rng_state_;
    }

    void ShufflePool(std::vector<std::string>& v) {
        for (size_t i = v.size(); i > 1; --i) {
            size_t j = NextRand() % i;
            std::swap(v[i - 1], v[j]);
        }
    }

    void BeginWithDeck(std::vector<std::string>&& deck, int round_seconds) {
        deck_ = std::move(deck);
        deck_idx_ = 0;
        correct_count_ = 0;
        skipped_count_ = 0;
        guessed_words_.clear();
        countdown_ms_ = 3000;
        remaining_ms_ = std::clamp(round_seconds, 15, 180) * 1000;
        flash_ms_ = 0;
        phase_ = Phase::kCountdown;
    }

    uint32_t rng_state_ = 0x12345678u;
    Phase phase_ = Phase::kInactive;
    std::string topic_title_;
    std::vector<std::string> deck_;
    size_t deck_idx_ = 0;
    int countdown_ms_ = 0;
    int remaining_ms_ = 60000;
    int flash_ms_ = 0;
    int correct_count_ = 0;
    int skipped_count_ = 0;
    std::vector<std::string> guessed_words_;
};

struct TriviaCardPayload {
    std::string question;
    std::vector<std::string> options;  // 2..4 options
    int correct_index = 0;
    std::string player_turn;
    std::string score_summary;
};

// Thread-safe bridge between GeminiLiveProtocol worker threads and the LVGL display thread.
class RobiGameHub {
public:
    static RobiGameHub& GetInstance() {
        static RobiGameHub instance;
        return instance;
    }

    void QueueTriviaCard(TriviaCardPayload card) {
        if (card.options.size() < 2) return;
        if (card.options.size() > 4) card.options.resize(4);
        card.correct_index = std::clamp(card.correct_index, 0,
                                        static_cast<int>(card.options.size()) - 1);
        card.question = Utf8SafeTruncate(card.question, 140);
        card.player_turn = Utf8SafeTruncate(card.player_turn, 36);
        card.score_summary = Utf8SafeTruncate(card.score_summary, 64);
        for (auto& opt : card.options) {
            opt = Utf8SafeTruncate(opt, 44);
        }

        std::lock_guard<std::mutex> lk(mutex_);
        pending_trivia_ = std::move(card);
        has_pending_trivia_ = true;
    }

    bool PopPendingTriviaCard(TriviaCardPayload& out) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!has_pending_trivia_) return false;
        out = std::move(pending_trivia_);
        has_pending_trivia_ = false;
        return true;
    }

    void QueueCustomHeadsUp(const std::string& topic, const std::vector<std::string>& words) {
        if (words.empty()) return;
        std::lock_guard<std::mutex> lk(mutex_);
        pending_headsup_topic_ = Utf8SafeTruncate(topic, 40);
        pending_headsup_words_.clear();
        for (const auto& w : words) {
            if (pending_headsup_words_.size() >= 25) break;
            std::string clean = Utf8SafeTruncate(w, 40);
            if (!clean.empty()) {
                pending_headsup_words_.push_back(std::move(clean));
            }
        }
        has_pending_headsup_ = !pending_headsup_words_.empty();
    }

    bool PopPendingCustomHeadsUp(std::string& topic_out, std::vector<std::string>& words_out) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!has_pending_headsup_) return false;
        topic_out = std::move(pending_headsup_topic_);
        words_out = std::move(pending_headsup_words_);
        has_pending_headsup_ = false;
        return true;
    }

private:
    RobiGameHub() = default;

    std::mutex mutex_;
    bool has_pending_trivia_ = false;
    TriviaCardPayload pending_trivia_;

    bool has_pending_headsup_ = false;
    std::string pending_headsup_topic_;
    std::vector<std::string> pending_headsup_words_;
};

}  // namespace robi

#endif  // ROBI_GAMES_H_
