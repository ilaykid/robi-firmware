#ifndef ROBI_GAMES_H_
#define ROBI_GAMES_H_

#include "robi_utf8.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
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

struct TriviaStaticDef {
    const char* q;
    const char* o0;  // Canonical correct answer before runtime Fisher-Yates option shuffle
    const char* o1;
    const char* o2;
    const char* o3;
};

struct TriviaCategoryDef {
    const char* id;
    const char* title_he;
    const char* title_en;
    const char* ai_topic_he;
    const char* ai_topic_en;
    uint32_t color_hex;
    const TriviaStaticDef* items_he;
    size_t count_he;
    const TriviaStaticDef* items_en;
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

// -------------------- Built-in Offline / Fallback Trivia Banks --------------------
inline constexpr TriviaStaticDef kTrivAnimalsHe[] = {
    {"כמה לבבות יש לתמנון?", "שלושה לבבות", "לב אחד", "שני לבבות", "חמישה לבבות"},
    {"איזו חיה היא המהירה ביותר ביבשה?", "ברדלס (צ'יטה)", "אריה", "סוס מרוץ", "זאב"},
    {"איזה עוף שוחה מצוין אך לא עף?", "פינגווין", "נשר", "ינשוף", "חסידה"},
    {"איזו חיה מסוגלת לישון בעמידה?", "סוס", "חתול", "קיפוד", "ארנב"},
    {"מה אוכלת פנדה ענקית רוב היום?", "במבוק", "דגים", "בננות", "בוטנים"},
    {"כמה רגליים יש לעכביש?", "שמונה", "שש", "עשר", "ארבע"},
    {"איזה יונק מסוגל לעוף באמת?", "עטלף", "סנאי", "יען", "קואלה"},
    {"מהי החיה הגדולה ביותר בעולם?", "לווייתן כחול", "פיל אפריקאי", "כריש לבן", "ג'ירפה"},
    {"באיזה איבר הנחש נעזר כדי להריח?", "בלשון", "בזנב", "בעיניים", "בקשקשים"},
};
inline constexpr TriviaStaticDef kTrivAnimalsEn[] = {
    {"How many hearts does an octopus have?", "Three", "One", "Two", "Five"},
    {"Which is the fastest land animal?", "Cheetah", "Lion", "Racehorse", "Wolf"},
    {"Which bird swims great but cannot fly?", "Penguin", "Eagle", "Owl", "Pelican"},
    {"What do giant pandas eat most of the day?", "Bamboo", "Fish", "Bananas", "Peanuts"},
    {"How many legs does a spider have?", "Eight", "Six", "Ten", "Four"},
    {"What is the largest animal on Earth?", "Blue Whale", "Elephant", "Great Shark", "Giraffe"},
};

inline constexpr TriviaStaticDef kTrivScienceHe[] = {
    {"איזה כוכב לכת נקרא הכוכב האדום?", "מאדים", "נגה", "צדק", "שבתאי"},
    {"סביב מה מקיף כדור הארץ?", "השמש", "הירח", "מאדים", "כוכב הצפון"},
    {"לאיזה כוכב לכת יש טבעות מפורסמות?", "שבתאי", "חמה", "מאדים", "נגה"},
    {"איזה גז אנו חייבים לנשום כדי לחיות?", "חמצן", "הליום", "מימן", "ארגון"},
    {"באיזו טמפרטורה מים קופאים לקרח?", "0 מעלות", "10 מעלות", "100 מעלות", "50 מעלות"},
    {"מהו כוכב הלכת הגדול במערכת השמש?", "צדק", "שבתאי", "כדור הארץ", "נפטון"},
    {"איזה כוח מושך חפצים אל האדמה?", "כוח הכבידה", "מגנטיות", "חשמל", "רוח"},
    {"מי היה האדם הראשון שצעד על הירח?", "ניל ארמסטרונג", "איינשטיין", "גלילאו", "ניוטון"},
    {"ממה עשויים העננים בשמיים?", "טיפות מים זעירות", "צמר גפן", "עשן לבן", "אבק כוכבים"},
};
inline constexpr TriviaStaticDef kTrivScienceEn[] = {
    {"Which planet is called the Red Planet?", "Mars", "Venus", "Jupiter", "Saturn"},
    {"Which planet is famous for its big rings?", "Saturn", "Mercury", "Mars", "Venus"},
    {"What gas do humans need to breathe?", "Oxygen", "Helium", "Hydrogen", "Neon"},
    {"At what Celsius temperature does water freeze?", "0 degrees", "10 degrees", "100 degrees", "32 degrees"},
    {"What is the largest planet in our solar system?", "Jupiter", "Saturn", "Earth", "Neptune"},
    {"Who was the first person to walk on the Moon?", "Neil Armstrong", "Einstein", "Galileo", "Newton"},
};

inline constexpr TriviaStaticDef kTrivSportsHe[] = {
    {"כמה שחקנים לקבוצה על המגרש בכדורגל?", "11 שחקנים", "5 שחקנים", "7 שחקנים", "9 שחקנים"},
    {"כמה שחקנים לקבוצה על המגרש בכדורסל?", "5 שחקנים", "11 שחקנים", "8 שחקנים", "3 שחקנים"},
    {"באיזה ספורט חובטים בכדור צהוב מעל רשת?", "טניס", "כדורעף", "ג'ודו", "שחייה"},
    {"כל כמה שנים נערכת אולימפיאדת הקיץ?", "כל 4 שנים", "כל שנה", "כל שנתיים", "כל 10 שנים"},
    {"כמה טבעות יש בסמל האולימפי?", "5 טבעות", "3 טבעות", "6 טבעות", "8 טבעות"},
    {"איזו מדליה מקבל המקום הראשון?", "מדליית זהב", "מדליית כסף", "מדליית ארד", "מדליית ברזל"},
    {"כמה נקודות שווה קליעה מחוץ לקשת בכדורסל?", "3 נקודות", "2 נקודות", "4 נקודות", "נקודה אחת"},
    {"מי השחקן שמותר לו לתפוס כדור ביד ברחבה?", "השוער", "החלוץ", "הקפטן", "המגן"},
    {"באיזה ענף זכתה לינוי אשרם בזהב אולימפי?", "התעמלות אמנותית", "שחייה", "טניס", "ג'ודו"},
};
inline constexpr TriviaStaticDef kTrivSportsEn[] = {
    {"How many players per team on a soccer field?", "11 players", "5 players", "7 players", "9 players"},
    {"How many players per team on a basketball court?", "5 players", "11 players", "8 players", "3 players"},
    {"How often are the Summer Olympics held?", "Every 4 years", "Every year", "Every 2 years", "Every 8 years"},
    {"How many rings are on the Olympic flag?", "5 rings", "3 rings", "6 rings", "8 rings"},
    {"What medal does first place win?", "Gold medal", "Silver medal", "Bronze medal", "Iron medal"},
    {"How many points is a shot beyond the arc?", "3 points", "2 points", "4 points", "1 point"},
};

inline constexpr TriviaStaticDef kTrivWorldHe[] = {
    {"מהי עיר הבירה של ישראל?", "ירושלים", "תל אביב", "חיפה", "אילת"},
    {"מהו המקום הנמוך ביותר בעולם?", "ים המלח", "הכנרת", "אילת", "הים התיכון"},
    {"באיזו עיר נמצא מגדל אייפל?", "פריז", "לונדון", "רומא", "ניו יורק"},
    {"באיזו מדינה נמצאות הפירמידות העתיקות?", "מצרים", "יוון", "איטליה", "סין"},
    {"מהי היבשת הגדולה ביותר בעולם?", "אסיה", "אירופה", "אפריקה", "אמריקה"},
    {"באיזו עיר בישראל יש שונית אלמוגים?", "אילת", "טבריה", "נתניה", "צפת"},
    {"מהו ההר הגבוה ביותר בישראל?", "הר החרמון", "הר תבור", "הר הכרמל", "מצדה"},
    {"איזה ממתק מפורסם מכינים משומשום (טחינה)?", "חלווה", "שוקולד", "מרציפן", "סוכרייה"},
    {"כמה צבעים סופרים בקשת בענן?", "7 צבעים", "4 צבעים", "10 צבעים", "12 צבעים"},
};
inline constexpr TriviaStaticDef kTrivWorldEn[] = {
    {"What is the lowest place on Earth's surface?", "Dead Sea", "Grand Canyon", "Red Sea", "Sahara"},
    {"In which city is the Eiffel Tower?", "Paris", "London", "Rome", "Madrid"},
    {"In which country are the ancient Pyramids?", "Egypt", "Greece", "Italy", "Peru"},
    {"What is the largest continent on Earth?", "Asia", "Europe", "Africa", "Australia"},
    {"Which sweet treat is made from sesame tahini?", "Halva", "Chocolate", "Toffee", "Gelato"},
    {"How many colors are in a traditional rainbow?", "7 colors", "4 colors", "9 colors", "12 colors"},
};

inline constexpr TriviaStaticDef kTrivRiddlesHe[] = {
    {"למי יש שיניים רבות אבל הוא לא אוכל?", "מסרק", "תנין", "כריש", "מזלג"},
    {"מה עולה למעלה כשיורד גשם?", "מטרייה", "ענן", "שלולית", "מגפיים"},
    {"למי יש מחוגים אבל אין לו ידיים?", "שעון", "רובוט", "מראה", "טלפון"},
    {"מה נשבר ברגע שאומרים את שמו?", "השקט", "זכוכית", "קרח", "בלון"},
    {"איזה חודש בשנה הוא הקצר ביותר?", "פברואר", "ינואר", "אוגוסט", "מאי"},
    {"כמה הם 7 כפול 8?", "56", "54", "48", "64"},
    {"איזה צבע יוצא מערבוב כחול וצהוב?", "ירוק", "סגול", "כתום", "ורוד"},
    {"שייך לך, אבל חברים משתמשים בו יותר ממך?", "השם שלך", "התיק שלך", "המיטה שלך", "האופניים"},
    {"למה יש צוואר אבל אין לו ראש?", "בקבוק", "ג'ירפה", "כובע", "כיסא"},
};
inline constexpr TriviaStaticDef kTrivRiddlesEn[] = {
    {"What has many teeth but never eats?", "A comb", "A crocodile", "A shark", "A fork"},
    {"What goes up as soon as the rain comes down?", "An umbrella", "A cloud", "A puddle", "Rainboots"},
    {"What gets broken the moment you say its name?", "Silence", "Glass", "Ice", "A balloon"},
    {"Which month of the year is the shortest?", "February", "January", "August", "May"},
    {"What color do you get mixing blue and yellow?", "Green", "Purple", "Orange", "Pink"},
    {"What belongs to you, yet others use it more?", "Your name", "Your bag", "Your bike", "Your shoes"},
};

}  // namespace detail

inline constexpr WordCategoryDef kHeadsUpCategories[5] = {
    {"animals", "חיות", "Animals", 0x10B981,
     detail::kAnimalsHe, sizeof(detail::kAnimalsHe) / sizeof(detail::kAnimalsHe[0]),
     detail::kAnimalsEn, sizeof(detail::kAnimalsEn) / sizeof(detail::kAnimalsEn[0])},
    {"food", "אוכל", "Food", 0xF59E0B,
     detail::kFoodHe, sizeof(detail::kFoodHe) / sizeof(detail::kFoodHe[0]),
     detail::kFoodEn, sizeof(detail::kFoodEn) / sizeof(detail::kFoodEn[0])},
    {"actions", "ספורט", "Sports", 0x00E5FF,
     detail::kSportsActionsHe, sizeof(detail::kSportsActionsHe) / sizeof(detail::kSportsActionsHe[0]),
     detail::kSportsActionsEn, sizeof(detail::kSportsActionsEn) / sizeof(detail::kSportsActionsEn[0])},
    {"chars", "דמויות", "Characters", 0xA855F7,
     detail::kCharactersHe, sizeof(detail::kCharactersHe) / sizeof(detail::kCharactersHe[0]),
     detail::kCharactersEn, sizeof(detail::kCharactersEn) / sizeof(detail::kCharactersEn[0])},
    {"objects", "חפצים", "Objects", 0xFF5C8A,
     detail::kObjectsPlacesHe, sizeof(detail::kObjectsPlacesHe) / sizeof(detail::kObjectsPlacesHe[0]),
     detail::kObjectsPlacesEn, sizeof(detail::kObjectsPlacesEn) / sizeof(detail::kObjectsPlacesEn[0])},
};

inline constexpr TriviaCategoryDef kTriviaCategories[5] = {
    {"animals", "חיות וטבע", "Animals", "חיות, טבע ויצורים מופלאים", "animals, wildlife, and nature", 0x10B981,
     detail::kTrivAnimalsHe, sizeof(detail::kTrivAnimalsHe) / sizeof(detail::kTrivAnimalsHe[0]),
     detail::kTrivAnimalsEn, sizeof(detail::kTrivAnimalsEn) / sizeof(detail::kTrivAnimalsEn[0])},
    {"science", "מדע וחלל", "Space & Sci", "חלל, מדע, המצאות וכוכבי לכת", "space, planets, science, and inventions", 0x00E5FF,
     detail::kTrivScienceHe, sizeof(detail::kTrivScienceHe) / sizeof(detail::kTrivScienceHe[0]),
     detail::kTrivScienceEn, sizeof(detail::kTrivScienceEn) / sizeof(detail::kTrivScienceEn[0])},
    {"sports", "ספורט", "Sports", "כדורגל, כדורסל, אולימפיאדה ושיאי ספורט", "soccer, basketball, Olympics, and sports", 0xF59E0B,
     detail::kTrivSportsHe, sizeof(detail::kTrivSportsHe) / sizeof(detail::kTrivSportsHe[0]),
     detail::kTrivSportsEn, sizeof(detail::kTrivSportsEn) / sizeof(detail::kTrivSportsEn[0])},
    {"world", "העולם שלנו", "Our World", "ישראל, מדינות, גאוגרפיה ומאכלים בעולם", "world geography, landmarks, and cultures", 0xA855F7,
     detail::kTrivWorldHe, sizeof(detail::kTrivWorldHe) / sizeof(detail::kTrivWorldHe[0]),
     detail::kTrivWorldEn, sizeof(detail::kTrivWorldEn) / sizeof(detail::kTrivWorldEn[0])},
    {"riddles", "חידות וכיף", "Riddles", "חידות היגיון, שאלות מחשבה וידע כללי משעשע", "clever riddles, brainteasers, and fun facts", 0xFF5C8A,
     detail::kTrivRiddlesHe, sizeof(detail::kTrivRiddlesHe) / sizeof(detail::kTrivRiddlesHe[0]),
     detail::kTrivRiddlesEn, sizeof(detail::kTrivRiddlesEn) / sizeof(detail::kTrivRiddlesEn[0])},
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
            topic_title_ = english ? "Mega Mix" : "ערבוב הכל";
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

struct TriviaQuestionItem {
    std::string question;
    std::vector<std::string> options;  // 2..4 options
    int correct_index = 0;
};

// 100% deterministic C++ state machine for Solo & 2-Player Touch Trivia (8 questions per round).
// Guarantees every single question card (1/8 -> 8/8) appears on screen with shuffled answer slots,
// deterministic turn rotation, live score tracking, and Winner Summary — zero reliance on LLM tool calls!
class TriviaEngine {
public:
    enum class Phase : uint8_t {
        kInactive = 0,
        kLoading = 1,
        kQuestion = 2,
        kReveal = 3,
        kSummary = 4,
    };

    struct TickEvents {
        bool advanced_to_next_question = false;
        bool finished_to_summary = false;
        bool loading_timed_out = false;
    };

    void SeedRng(uint32_t seed) {
        rng_state_ = (seed == 0) ? 0xA5A5F00Du : seed;
    }

    void BeginLoading(const std::string& topic_title, bool head_to_head,
                      const std::string& p1, const std::string& p2, bool english) {
        ConfigurePlayersAndReset(topic_title, head_to_head, p1, p2, english);
        deck_.clear();
        loading_ms_ = 0;
        phase_ = Phase::kLoading;
    }

    void StartOfflineDeck(int cat_index, bool head_to_head,
                          const std::string& p1, const std::string& p2,
                          bool english, size_t round_questions = 8) {
        std::string title;
        if (cat_index >= 0 && cat_index < 5) {
            title = english ? kTriviaCategories[cat_index].title_en
                            : kTriviaCategories[cat_index].title_he;
        } else {
            title = english ? "Mega Mix" : "ערבוב הכל";
        }
        ConfigurePlayersAndReset(title, head_to_head, p1, p2, english);
        std::vector<TriviaQuestionItem> pool = BuildOfflinePool(cat_index, english);
        ShuffleQuestions(pool, /*preserve_first=*/false);
        size_t take = std::min(std::max<size_t>(2, round_questions), pool.size());
        pool.resize(take);
        for (auto& q : pool) {
            ShuffleOptions(q);
        }
        deck_ = std::move(pool);
        phase_ = deck_.empty() ? Phase::kInactive : Phase::kQuestion;
    }

    void StartCustomDeck(const std::string& topic_title,
                         const std::vector<TriviaQuestionItem>& custom_items,
                         int fallback_cat_index, bool head_to_head,
                         const std::string& p1, const std::string& p2,
                         bool english, size_t round_questions = 8,
                         bool preserve_first_question_order = false) {
        std::string clean_title = topic_title.empty()
                                      ? (english ? "Custom Trivia" : "טריוויה חיה")
                                      : Utf8SafeTruncate(topic_title, 36);
        ConfigurePlayersAndReset(clean_title, head_to_head, p1, p2, english);

        std::vector<TriviaQuestionItem> valid;
        valid.reserve(round_questions);
        for (const auto& raw : custom_items) {
            TriviaQuestionItem cleaned = SanitizeItem(raw);
            if (!cleaned.question.empty() && cleaned.options.size() >= 2) {
                valid.push_back(std::move(cleaned));
            }
            if (valid.size() >= round_questions) break;
        }

        if (valid.size() < 4) {
            // Seamlessly pad/fallback from built-in curated bank so game NEVER fails
            std::vector<TriviaQuestionItem> fallback = BuildOfflinePool(fallback_cat_index, english);
            ShuffleQuestions(fallback, false);
            for (auto& fb : fallback) {
                if (valid.size() >= round_questions) break;
                valid.push_back(std::move(fb));
            }
        }

        ShuffleQuestions(valid, preserve_first_question_order);
        if (valid.size() > round_questions) {
            valid.resize(round_questions);
        }
        for (auto& q : valid) {
            ShuffleOptions(q);
        }
        deck_ = std::move(valid);
        phase_ = deck_.empty() ? Phase::kInactive : Phase::kQuestion;
    }

    void Stop() {
        phase_ = Phase::kInactive;
        deck_.clear();
        q_index_ = 0;
        loading_ms_ = 0;
        reveal_ms_ = 0;
    }

    bool IsActive() const { return phase_ != Phase::kInactive; }
    Phase phase() const { return phase_; }
    const std::string& topic_title() const { return topic_title_; }
    bool head_to_head() const { return head_to_head_; }
    bool english() const { return english_; }
    size_t question_index() const { return q_index_; }
    size_t total_questions() const { return deck_.size(); }

    int CurrentPlayerIndex() const {
        return head_to_head_ ? static_cast<int>(q_index_ % 2) : 0;
    }

    const std::string& CurrentPlayerName() const {
        return player_names_[CurrentPlayerIndex()];
    }

    const std::string& player_name(int idx) const {
        return player_names_[idx & 1];
    }

    int score(int idx) const {
        return scores_[idx & 1];
    }

    const TriviaQuestionItem* CurrentQuestion() const {
        if (q_index_ >= deck_.size()) return nullptr;
        return &deck_[q_index_];
    }

    bool last_was_correct() const { return last_was_correct_; }
    int last_picked_index() const { return last_picked_index_; }
    int last_answered_player() const { return last_answered_player_; }
    const std::string& last_chosen_text() const { return last_chosen_text_; }
    const std::string& last_correct_text() const { return last_correct_text_; }

    bool SubmitAnswer(int picked_idx) {
        if (phase_ != Phase::kQuestion || q_index_ >= deck_.size()) {
            return false;
        }
        const auto& cur = deck_[q_index_];
        if (picked_idx < 0 || picked_idx >= static_cast<int>(cur.options.size())) {
            return false;
        }
        int p_idx = CurrentPlayerIndex();
        last_answered_player_ = p_idx;
        last_picked_index_ = picked_idx;
        last_was_correct_ = (picked_idx == cur.correct_index);
        last_chosen_text_ = cur.options[static_cast<size_t>(picked_idx)];
        last_correct_text_ = cur.options[static_cast<size_t>(cur.correct_index)];
        if (last_was_correct_) {
            ++scores_[p_idx];
        }
        reveal_ms_ = 1450;
        phase_ = Phase::kReveal;
        return true;
    }

    TickEvents Step(int dt_ms) {
        TickEvents ev = {};
        if (dt_ms <= 0 || phase_ == Phase::kInactive || phase_ == Phase::kSummary) {
            return ev;
        }
        if (phase_ == Phase::kLoading) {
            loading_ms_ += dt_ms;
            if (loading_ms_ >= 7200) {
                ev.loading_timed_out = true;
            }
            return ev;
        }
        if (phase_ == Phase::kReveal) {
            reveal_ms_ = std::max(0, reveal_ms_ - dt_ms);
            if (reveal_ms_ == 0) {
                ++q_index_;
                if (q_index_ < deck_.size()) {
                    phase_ = Phase::kQuestion;
                    ev.advanced_to_next_question = true;
                } else {
                    phase_ = Phase::kSummary;
                    ev.finished_to_summary = true;
                }
            }
        }
        return ev;
    }

    std::string FormatHeaderText() const {
        if (phase_ == Phase::kLoading) {
            return topic_title_;
        }
        if (phase_ == Phase::kSummary) {
            if (head_to_head_) {
                if (scores_[0] > scores_[1]) {
                    return english_ ? ("Winner: " + player_names_[0] + "!")
                                    : ("המנצח: " + player_names_[0] + "!");
                }
                if (scores_[1] > scores_[0]) {
                    return english_ ? ("Winner: " + player_names_[1] + "!")
                                    : ("המנצח: " + player_names_[1] + "!");
                }
                return english_ ? "Awesome Tie!" : "תיקו אדיר!";
            }
            return english_ ? "Quiz Complete!" : "סיום החידון!";
        }

        char buf[128];
        int q_num = static_cast<int>(q_index_ + 1);
        int q_tot = static_cast<int>(deck_.size());
        if (head_to_head_) {
            std::snprintf(buf, sizeof(buf),
                          english_ ? "%s's Turn  •  %d/%d"
                                   : "תור %s  •  %d/%d",
                          CurrentPlayerName().c_str(), q_num, q_tot);
        } else {
            std::snprintf(buf, sizeof(buf),
                          english_ ? "Q %d/%d  •  Score: %d"
                                   : "שאלה %d/%d  •  ניקוד: %d",
                          q_num, q_tot, scores_[0]);
        }
        return std::string(buf);
    }

    std::string FormatScoreText() const {
        if (!head_to_head_ || phase_ == Phase::kLoading || phase_ == Phase::kSummary) {
            return {};
        }
        char buf[128];
        std::snprintf(buf, sizeof(buf), "%s: %d   •   %s: %d",
                      player_names_[0].c_str(), scores_[0],
                      player_names_[1].c_str(), scores_[1]);
        return std::string(buf);
    }

    std::string FormatSummaryBody() const {
        char buf[160];
        if (head_to_head_) {
            std::snprintf(buf, sizeof(buf),
                          english_ ? "%s: %d   |   %s: %d\nGreat battle! Play again?"
                                   : "%s: %d   •   %s: %d\nמשחק אלופים! עוד סיבוב?",
                          player_names_[0].c_str(), scores_[0],
                          player_names_[1].c_str(), scores_[1]);
        } else {
            std::snprintf(buf, sizeof(buf),
                          english_ ? "You got %d out of %d!\nPlay another round?"
                                   : "ענית נכון על %d מתוך %d שאלות!\nעוד סיבוב?",
                          scores_[0], static_cast<int>(deck_.size()));
        }
        return std::string(buf);
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

    void ConfigurePlayersAndReset(const std::string& title, bool head_to_head,
                                  const std::string& p1, const std::string& p2, bool english) {
        topic_title_ = Utf8SafeTruncate(title, 36);
        head_to_head_ = head_to_head;
        english_ = english;
        player_names_[0] = p1.empty() ? (english ? "Player 1" : "שחקן 1") : Utf8SafeTruncate(p1, 20);
        player_names_[1] = p2.empty() ? (english ? "Player 2" : "שחקן 2") : Utf8SafeTruncate(p2, 20);
        scores_[0] = 0;
        scores_[1] = 0;
        q_index_ = 0;
        loading_ms_ = 0;
        reveal_ms_ = 0;
        last_was_correct_ = false;
        last_picked_index_ = -1;
        last_answered_player_ = 0;
        last_chosen_text_.clear();
        last_correct_text_.clear();
    }

    static TriviaQuestionItem SanitizeItem(const TriviaQuestionItem& raw) {
        TriviaQuestionItem out;
        out.question = Utf8SafeTruncate(raw.question, 120);
        for (const auto& opt : raw.options) {
            if (out.options.size() >= 4) break;
            std::string c = Utf8SafeTruncate(opt, 38);
            if (!c.empty()) {
                out.options.push_back(std::move(c));
            }
        }
        if (out.options.size() >= 2) {
            out.correct_index = std::clamp(raw.correct_index, 0,
                                           static_cast<int>(out.options.size()) - 1);
        }
        return out;
    }

    static std::vector<TriviaQuestionItem> BuildOfflinePool(int cat_index, bool english) {
        std::vector<TriviaQuestionItem> pool;
        auto append_cat = [&](const TriviaCategoryDef& cat) {
            const TriviaStaticDef* arr = english ? cat.items_en : cat.items_he;
            size_t n = english ? cat.count_en : cat.count_he;
            for (size_t i = 0; i < n; ++i) {
                TriviaQuestionItem item;
                item.question = arr[i].q;
                item.options = {arr[i].o0, arr[i].o1, arr[i].o2, arr[i].o3};
                item.correct_index = 0;
                pool.push_back(std::move(item));
            }
        };
        if (cat_index >= 0 && cat_index < 5) {
            append_cat(kTriviaCategories[cat_index]);
        } else {
            for (const auto& cat : kTriviaCategories) {
                append_cat(cat);
            }
        }
        return pool;
    }

    void ShuffleQuestions(std::vector<TriviaQuestionItem>& v, bool preserve_first) {
        size_t start = (preserve_first && v.size() > 1) ? 1 : 0;
        for (size_t i = v.size(); i > start + 1; --i) {
            size_t span = i - start;
            size_t j = start + (NextRand() % span);
            std::swap(v[i - 1], v[j]);
        }
    }

    void ShuffleOptions(TriviaQuestionItem& item) {
        if (item.options.size() < 2) return;
        int corr = std::clamp(item.correct_index, 0, static_cast<int>(item.options.size()) - 1);
        for (size_t i = item.options.size(); i > 1; --i) {
            size_t j = NextRand() % i;
            if (static_cast<int>(i - 1) == corr) {
                corr = static_cast<int>(j);
            } else if (static_cast<int>(j) == corr) {
                corr = static_cast<int>(i - 1);
            }
            std::swap(item.options[i - 1], item.options[j]);
        }
        item.correct_index = corr;
    }

    uint32_t rng_state_ = 0x9E3779B9u;
    Phase phase_ = Phase::kInactive;
    std::string topic_title_;
    bool head_to_head_ = false;
    bool english_ = false;
    std::string player_names_[2] = {"שחקן 1", "שחקן 2"};
    int scores_[2] = {0, 0};
    std::vector<TriviaQuestionItem> deck_;
    size_t q_index_ = 0;
    int loading_ms_ = 0;
    int reveal_ms_ = 0;
    bool last_was_correct_ = false;
    int last_picked_index_ = -1;
    int last_answered_player_ = 0;
    std::string last_chosen_text_;
    std::string last_correct_text_;
};

struct VoiceTriviaStartCmd {
    std::string topic;
    bool head_to_head = false;
    std::vector<TriviaQuestionItem> questions;
};

// Thread-safe bridge between GeminiLiveProtocol worker threads and the LVGL display thread.
class RobiGameHub {
public:
    static RobiGameHub& GetInstance() {
        static RobiGameHub instance;
        return instance;
    }

    void SetMicMutedForScreenGame(bool muted) {
        mic_muted_for_game_.store(muted, std::memory_order_relaxed);
    }

    bool IsMicMutedForScreenGame() const {
        return mic_muted_for_game_.load(std::memory_order_relaxed);
    }

    uint32_t NextTriviaRequestId() {
        std::lock_guard<std::mutex> lk(mutex_);
        ++next_req_id_;
        if (next_req_id_ == 0) ++next_req_id_;
        has_delivered_deck_ = false;
        delivered_deck_.clear();
        return next_req_id_;
    }

    void DeliverAsyncTriviaDeck(uint32_t req_id, std::vector<TriviaQuestionItem> items) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (req_id == 0 || req_id != next_req_id_) {
            return;  // Stale request cancelled by user
        }
        delivered_req_id_ = req_id;
        delivered_deck_ = std::move(items);
        has_delivered_deck_ = true;
    }

    bool PopAsyncTriviaDeck(uint32_t expected_req_id, std::vector<TriviaQuestionItem>& out_items) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!has_delivered_deck_ || expected_req_id == 0 || delivered_req_id_ != expected_req_id) {
            return false;
        }
        out_items = std::move(delivered_deck_);
        has_delivered_deck_ = false;
        return true;
    }

    void QueueVoiceTriviaStart(VoiceTriviaStartCmd cmd) {
        cmd.topic = Utf8SafeTruncate(cmd.topic, 40);
        if (cmd.questions.size() > 12) {
            cmd.questions.resize(12);
        }
        std::lock_guard<std::mutex> lk(mutex_);
        pending_voice_trivia_ = std::move(cmd);
        has_pending_voice_trivia_ = true;
    }

    bool PopVoiceTriviaStart(VoiceTriviaStartCmd& out) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!has_pending_voice_trivia_) return false;
        out = std::move(pending_voice_trivia_);
        has_pending_voice_trivia_ = false;
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

    std::atomic<bool> mic_muted_for_game_{false};
    std::mutex mutex_;

    uint32_t next_req_id_ = 1;
    uint32_t delivered_req_id_ = 0;
    bool has_delivered_deck_ = false;
    std::vector<TriviaQuestionItem> delivered_deck_;

    bool has_pending_voice_trivia_ = false;
    VoiceTriviaStartCmd pending_voice_trivia_;

    bool has_pending_headsup_ = false;
    std::string pending_headsup_topic_;
    std::vector<std::string> pending_headsup_words_;
};

}  // namespace robi

#endif  // ROBI_GAMES_H_
