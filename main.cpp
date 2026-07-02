/**
 * @file main.cpp
 * @brief Trivia Quiz System - Player Application (App 1)
 * @author Student 1 - Ion Popescu
 *
 * All input comes through command-line arguments only (argc/argv).
 * No std::cin is used anywhere. Each command is one executable call.
 * Progress is saved to a file after every answer so the player can
 * resume across separate runs.
 *
 * Bidirectional file communication with App 2 (admin):
 *   questions.txt   -> written by admin, READ here
 *   leaderboard.txt -> WRITTEN here, read by admin
 *   progress.txt    -> WRITTEN here, read by admin
 *
 * Commands:
 *   ./main play <name>           Start or resume a game session
 *   ./main answer <name> <1-4>  Submit answer to current question
 *   ./main answer <name> 0      Save progress and quit
 *   ./main leaderboard          View scores sorted best first
 *   ./main questions            View all available questions
 *   ./main progress             View current saved progress
 *   ./main reset <name>         Erase saved progress
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <stdexcept>

using namespace std;

// ============================================================
// CLASS: Question
//
// Stores one trivia question: text, four answer options,
// the correct answer index (1-4), and a difficulty level (1-3).
//
// Demonstrates: class with constructor, methods, operator<<.
// Association: used inside GamePlayer via vector<Question>.
// ============================================================
class Question {
public:
    string text;        ///< The question shown to the player
    string options[4];  ///< The four answer choices
    int correct;        ///< Index of the correct answer: 1, 2, 3, or 4
    int difficulty;     ///< 1=Easy, 2=Medium, 3=Hard

    /**
     * @brief Default constructor.
     * Initialises numeric members to safe defaults.
     */
    Question() : correct(1), difficulty(1) {}

    /**
     * @brief Full constructor.
     * @param t    Question text
     * @param opts Four answer option strings
     * @param c    Correct answer index (1-4)
     * @param d    Difficulty (1-3)
     */
    Question(const string& t, const string opts[4], int c, int d)
        : text(t), correct(c), difficulty(d) {
        for (int i = 0; i < 4; i++) options[i] = opts[i];
    }

    /**
     * @brief Returns a human-readable difficulty label.
     * @return "Easy", "Medium", or "Hard"
     */
    string difficultyLabel() const {
        if (difficulty == 1) return "Easy";
        if (difficulty == 2) return "Medium";
        return "Hard";
    }

    /**
     * @brief Stream output operator overload.
     *
     * Enables: cout << question;
     * Used by displayList<Question> to print question banks.
     *
     * @param os  The output stream
     * @param q   The Question to display
     * @return    Reference to the stream (enables chaining)
     */
    friend ostream& operator<<(ostream& os, const Question& q) {
        os << "\n================================================\n";
        os << "Difficulty: " << q.difficultyLabel() << "\n";
        os << "================================================\n\n";
        os << q.text << "\n\n";
        for (int i = 0; i < 4; i++)
            os << i + 1 << ". " << q.options[i] << "\n";
        return os;
    }
};

// ============================================================
// CLASS: Player  (base class)
//
// Holds a player's name and score. Provides virtual methods
// so derived classes can extend save behaviour.
//
// Demonstrates: base class, virtual destructor, operator<<.
// Inheritance: GamePlayer IS-A Player.
// ============================================================
class Player {
protected:
    string name;  ///< Player's display name
    int score;    ///< Number of correct answers

public:
    /**
     * @brief Constructs a Player.
     * @param n Player name (default "Unknown")
     * @param s Starting score (default 0)
     */
    Player(const string& n = "Unknown", int s = 0)
        : name(n), score(s) {}

    /// @brief Virtual destructor for safe polymorphic deletion
    virtual ~Player() {}

    /// @brief Returns the player's name
    string getName() const { return name; }

    /// @brief Returns the player's current score
    int getScore() const { return score; }

    /// @brief Sets the score to a specific value
    void setScore(int s) { score = s; }

    /// @brief Increments score by one on a correct answer
    void incrementScore() { score++; }

    /**
     * @brief Appends player name and score to leaderboard.txt.
     *
     * Virtual so a subclass can override if needed.
     * Called when the quiz is completed.
     */
    virtual void saveToLeaderboard() const {
        ofstream file("leaderboard.txt", ios::app);
        if (file.is_open())
            file << name << " " << score << "\n";
    }

    /**
     * @brief Stream output operator overload.
     *
     * Enables: cout << player;
     * Used by displayList<Player> to print leaderboard rows.
     *
     * @param os  The output stream
     * @param p   The Player to display
     * @return    Reference to the stream
     */
    friend ostream& operator<<(ostream& os, const Player& p) {
        os << p.name << " - Score: " << p.score;
        return os;
    }
};

// ============================================================
// CLASS: GamePlayer  (inherits Player)
//
// Extends Player with per-question progress persistence so
// the game can span multiple program invocations without
// using any keyboard input.
//
// Demonstrates:
//   Inheritance  : GamePlayer IS-A Player
//   Association  : HAS-A vector<Question> (used in commands)
//   Smart pointer: managed via unique_ptr<GamePlayer> in main
// ============================================================
class GamePlayer : public Player {
private:
    int questionIndex;  ///< 0-based index of next unanswered question

public:
    /**
     * @brief Constructs a GamePlayer.
     * @param n   Player name
     * @param s   Current score (default 0)
     * @param idx Current question position (default 0)
     */
    GamePlayer(const string& n, int s = 0, int idx = 0)
        : Player(n, s), questionIndex(idx) {}

    /// @brief Returns index of the next unanswered question
    int getQuestionIndex() const { return questionIndex; }

    /// @brief Moves to the next question
    void advanceQuestion() { questionIndex++; }

    /**
     * @brief Persists current game state to progress.txt.
     *
     * Format written: <name> <questionIndex> <score>
     * Called after every answered question. This file is also
     * read by App 2 (admin) — bidirectional communication.
     */
    void saveProgress() const {
        ofstream file("progress.txt");
        if (file.is_open())
            file << name << " " << questionIndex << " " << score;
    }

    /**
     * @brief Loads saved progress for this player from progress.txt.
     *
     * Reads the file and checks if the stored name matches.
     * If matched, restores questionIndex and score.
     *
     * @return true if valid matching progress was found and loaded
     */
    bool loadProgress() {
        ifstream file("progress.txt");
        if (!file.is_open()) return false;

        string savedName;
        int savedIdx = 0, savedScore = 0;

        if (file >> savedName >> savedIdx >> savedScore) {
            if (savedName == name && savedName != "none") {
                questionIndex = savedIdx;
                score = savedScore;
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Writes the empty sentinel to progress.txt.
     *
     * Called when a game is completed or explicitly reset.
     * Sentinel value "none 0 0" signals no active session.
     */
    static void clearProgress() {
        ofstream file("progress.txt");
        if (file.is_open()) file << "none 0 0";
    }
};

// ============================================================
// TEMPLATE FUNCTION: displayList<T>
//
// Prints a numbered list of any type T that has operator<<.
// Works for both vector<Question> and vector<Player> without
// any code duplication - demonstrating C++ templates.
//
// @tparam T     Item type (must have operator<< defined)
// @param items  Vector of items to display
// @param header Title printed above the list
// ============================================================
template <typename T>
void displayList(const vector<T>& items, const string& header) {
    cout << "\n================================================\n";
    cout << header << "\n";
    cout << "================================================\n";

    if (items.empty()) {
        cout << "(empty)\n";
        return;
    }

    for (size_t i = 0; i < items.size(); i++)
        cout << "\n[" << i + 1 << "] " << items[i] << "\n";
}

// ============================================================
// FUNCTION: loadQuestions
//
// Reads questions.txt written by App 2 (admin) and returns a
// vector of Question objects. This is the READ side of the
// bidirectional file communication between the two apps.
//
// @return Vector of all questions parsed from questions.txt
// ============================================================
vector<Question> loadQuestions() {
    vector<Question> questions;
    ifstream file("questions.txt");

    if (!file.is_open()) {
        cerr << "Error: could not open questions.txt\n";
        return questions;
    }

    Question q;
    while (getline(file, q.text)) {
        if (q.text.empty()) continue;
        if (!getline(file, q.options[0])) break;
        if (!getline(file, q.options[1])) break;
        if (!getline(file, q.options[2])) break;
        if (!getline(file, q.options[3])) break;
        file >> q.correct >> q.difficulty;
        file.ignore();
        questions.push_back(q);
    }
    return questions;
}

// ============================================================
// COMMAND: play <player_name>
//
// Starts a new session or resumes an existing one.
// Shows the current question. Player answers with the
// separate "answer" command (one argv call per question).
//
// Uses unique_ptr<GamePlayer> for smart memory management.
//
// @param playerName  Name passed via argv
// ============================================================
void cmdPlay(const string& playerName) {
    // Smart pointer: unique_ptr manages GamePlayer lifetime automatically
    unique_ptr<GamePlayer> player = make_unique<GamePlayer>(playerName);

    vector<Question> questions = loadQuestions();
    if (questions.empty()) {
        cout << "No questions available. Ask the admin to add some.\n";
        return;
    }

    bool resumed = player->loadProgress();
    int idx = player->getQuestionIndex();

    if (idx >= (int)questions.size()) {
        cout << "You already completed the quiz! Check the leaderboard.\n";
        return;
    }

    if (resumed) {
        cout << "Resuming from question " << idx + 1
             << " (score: " << player->getScore() << ")\n\n";
    } else {
        cout << "Starting new game for: " << playerName << "\n\n";
        player->saveProgress();
    }

    // Display current question
    cout << "Question " << idx + 1 << " / " << questions.size() << "\n";
    cout << questions[idx];
    cout << "\nSubmit answer: ./main answer " << playerName << " <1-4>\n";
    cout << "Save and quit: ./main answer " << playerName << " 0\n";
}

// ============================================================
// COMMAND: answer <player_name> <0-4>
//
// Submits one answer to the current question.
// Validates the answer, updates score, advances question index,
// and saves progress. If all done, writes to leaderboard.
// Answer 0 saves and quits without consuming a question.
//
// @param playerName  Player name from argv
// @param answer      0=quit, 1-4=answer choice
// ============================================================
void cmdAnswer(const string& playerName, int answer) {
    if (answer < 0 || answer > 4) {
        cerr << "Error: answer must be 0 (quit) or 1-4.\n";
        return;
    }

    // Smart pointer: unique_ptr manages GamePlayer lifetime automatically
    unique_ptr<GamePlayer> player = make_unique<GamePlayer>(playerName);

    if (!player->loadProgress()) {
        cerr << "No active game for '" << playerName << "'.\n";
        cerr << "Start one with: ./main play " << playerName << "\n";
        return;
    }

    vector<Question> questions = loadQuestions();
    int idx = player->getQuestionIndex();

    if (idx >= (int)questions.size()) {
        cout << "Quiz already complete for " << playerName << ".\n";
        return;
    }

    // Save and quit without consuming the question
    if (answer == 0) {
        player->saveProgress();
        cout << "Progress saved. Question " << idx + 1
             << " waiting, score " << player->getScore() << ".\n";
        return;
    }

    // Evaluate the answer
    if (answer == questions[idx].correct) {
        cout << "Correct!\n";
        player->incrementScore();
    } else {
        cout << "Wrong. Correct answer was " << questions[idx].correct
             << " - " << questions[idx].options[questions[idx].correct - 1] << "\n";
    }

    player->advanceQuestion();
    int next = player->getQuestionIndex();

    // Quiz complete
    if (next >= (int)questions.size()) {
        cout << "\n================================================\n";
        cout << "QUIZ COMPLETED!\n";
        cout << "================================================\n";
        cout << "Final Score: " << player->getScore()
             << " / " << questions.size() << "\n\n";
        player->saveToLeaderboard();
        GamePlayer::clearProgress();
        cout << "Score saved. View with: ./main leaderboard\n";
    } else {
        // Save and show next question
        player->saveProgress();
        cout << "\nQuestion " << next + 1 << " / " << questions.size() << "\n";
        cout << questions[next];
        cout << "\nSubmit answer: ./main answer " << playerName << " <1-4>\n";
    }
}

// ============================================================
// COMMAND: leaderboard
//
// Reads leaderboard.txt, sorts by score descending, and
// displays using the generic displayList<Player> template.
// ============================================================
void cmdLeaderboard() {
    ifstream file("leaderboard.txt");
    if (!file.is_open()) {
        cout << "Leaderboard is empty.\n";
        return;
    }

    vector<Player> players;
    string name;
    int score;

    while (file >> name >> score)
        players.emplace_back(name, score);

    sort(players.begin(), players.end(),
        [](const Player& a, const Player& b) {
            return a.getScore() > b.getScore();
        });

    displayList(players, "LEADERBOARD");
}

// ============================================================
// COMMAND: questions
//
// Loads and displays all questions using displayList<Question>.
// ============================================================
void cmdQuestions() {
    vector<Question> questions = loadQuestions();
    displayList(questions, "ALL QUESTIONS");
    cout << "\nTotal: " << questions.size() << " question(s)\n";
}

// ============================================================
// COMMAND: progress
//
// Shows the current saved progress from progress.txt.
// This file is also read by App 2 (bidirectional flow).
// ============================================================
void cmdProgress() {
    ifstream file("progress.txt");
    if (!file.is_open()) {
        cout << "No progress file found.\n";
        return;
    }

    string name;
    int idx, score;

    cout << "\n================================================\n";
    cout << "SAVED PROGRESS\n";
    cout << "================================================\n\n";

    if (file >> name >> idx >> score && name != "none") {
        cout << "Player: " << name << "\n";
        cout << "Next Question: " << idx + 1 << "\n";
        cout << "Score: " << score << "\n";
    } else {
        cout << "No active saved progress.\n";
    }
}

// ============================================================
// COMMAND: reset <player_name>
//
// Erases saved progress for the named player.
//
// @param playerName  The player whose progress to erase
// ============================================================
void cmdReset(const string& playerName) {
    ifstream check("progress.txt");
    string savedName;
    int idx, score;

    if (check >> savedName >> idx >> score && savedName == playerName) {
        check.close();
        GamePlayer::clearProgress();
        cout << "Progress for '" << playerName << "' has been reset.\n";
    } else {
        cout << "No saved progress found for '" << playerName << "'.\n";
    }
}

// ============================================================
// FUNCTION: printUsage
// Shows all available commands with syntax.
// ============================================================
void printUsage() {
    cout << "\n================================================\n";
    cout << "TRIVIA QUIZ SYSTEM - Player App\n";
    cout << "================================================\n\n";
    cout << "  ./main play <name>           Start or resume a game\n";
    cout << "  ./main answer <name> <1-4>   Submit answer\n";
    cout << "  ./main answer <name> 0       Save and quit\n";
    cout << "  ./main leaderboard           View leaderboard\n";
    cout << "  ./main questions             View all questions\n";
    cout << "  ./main progress              View saved progress\n";
    cout << "  ./main reset <name>          Reset saved progress\n\n";
    cout << "Example:\n";
    cout << "  ./main play Alice\n";
    cout << "  ./main answer Alice 3\n";
    cout << "  ./main answer Alice 1\n";
    cout << "  ./main leaderboard\n\n";
}

// ============================================================
// MAIN - Command-line argument dispatcher
//
// Parses argv and routes to the correct command function.
// Validates argument count before calling each handler.
// No std::cin used anywhere.
// ============================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 0;
    }

    string command = argv[1];

    if (command == "play") {
        if (argc < 3) { cerr << "Usage: ./main play <name>\n"; return 1; }
        cmdPlay(argv[2]);

    } else if (command == "answer") {
        if (argc < 4) { cerr << "Usage: ./main answer <name> <0-4>\n"; return 1; }
        try {
            cmdAnswer(argv[2], stoi(argv[3]));
        } catch (...) {
            cerr << "Error: answer must be a number (0-4)\n"; return 1;
        }

    } else if (command == "leaderboard") {
        cmdLeaderboard();

    } else if (command == "questions") {
        cmdQuestions();

    } else if (command == "progress") {
        cmdProgress();

    } else if (command == "reset") {
        if (argc < 3) { cerr << "Usage: ./main reset <name>\n"; return 1; }
        cmdReset(argv[2]);

    } else {
        cerr << "Unknown command: '" << command << "'\n";
        printUsage();
        return 1;
    }

    return 0;
}
