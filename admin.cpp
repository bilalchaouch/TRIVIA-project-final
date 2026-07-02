/**
 * @file admin.cpp
 * @brief Trivia Quiz System - Admin Application (App 2)
 * @author Student 2 - Maria Ionescu
 *
 * All input comes through command-line arguments only (argc/argv).
 * No std::cin is used anywhere. Each command is one executable call.
 *
 * Changes made here to questions.txt are immediately visible to
 * App 1 (main). This is the WRITE side of bidirectional file
 * communication for the question bank.
 *
 * Bidirectional file communication with App 1 (main):
 *   questions.txt   -> WRITTEN here, read by main
 *   leaderboard.txt -> written by main, READ here
 *   progress.txt    -> written by main, READ here
 *
 * Commands:
 *   ./admin view_questions
 *   ./admin add_question <text> <opt1> <opt2> <opt3> <opt4> <correct(1-4)> <diff(1-3)>
 *   ./admin delete_question <number>
 *   ./admin view_leaderboard
 *   ./admin delete_player <name>
 *   ./admin clear_leaderboard
 *   ./admin view_progress
 *   ./admin reset_progress
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
// Same schema as main.cpp so both apps share questions.txt
// in the same format (required for bidirectional file flow).
//
// Demonstrates: class, constructor, operator<<.
// ============================================================
class Question {
public:
    string text;        ///< The question text
    string options[4];  ///< Four answer choices
    int correct;        ///< Correct answer index: 1-4
    int difficulty;     ///< 1=Easy, 2=Medium, 3=Hard

    /**
     * @brief Default constructor.
     * Sets numeric members to safe defaults.
     */
    Question() : correct(1), difficulty(1) {}

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
     * Displays the question with all options and the correct answer.
     * Used by displayList<Question> in the admin panel.
     *
     * @param os  Output stream
     * @param q   Question to display
     * @return    Reference to the stream
     */
    friend ostream& operator<<(ostream& os, const Question& q) {
        os << "\n================================================\n";
        os << "Difficulty: " << q.difficultyLabel() << "\n";
        os << "================================================\n\n";
        os << q.text << "\n\n";
        for (int i = 0; i < 4; i++)
            os << i + 1 << ". " << q.options[i] << "\n";
        os << "\nCorrect Answer: " << q.correct << "\n";
        return os;
    }
};

// ============================================================
// CLASS: User  (base class)
//
// Represents a generic system user with a name and role.
// Virtual getRole() allows derived classes to identify themselves.
//
// Demonstrates: base class, virtual method.
// Inheritance: Admin IS-A User.
// ============================================================
class User {
protected:
    string name;  ///< The user's display name

public:
    /**
     * @brief Constructs a User.
     * @param n Display name (default "User")
     */
    explicit User(const string& n = "User") : name(n) {}

    /// @brief Virtual destructor for safe polymorphic deletion
    virtual ~User() {}

    /// @brief Returns the user's name
    string getName() const { return name; }

    /**
     * @brief Returns the role label for this user type.
     *
     * Virtual so derived classes can override and return
     * their own role string (e.g. "Admin").
     *
     * @return Role as a string
     */
    virtual string getRole() const { return "User"; }
};

// ============================================================
// CLASS: Admin  (inherits User)
//
// Extends User with password authentication and elevated access
// to the question bank and leaderboard management functions.
//
// Demonstrates:
//   Inheritance  : Admin IS-A User
//   Smart pointer: managed via unique_ptr<Admin> in main
// ============================================================
class Admin : public User {
public:
    /**
     * @brief Constructs an Admin.
     * @param n Admin name (default "Admin")
     */
    explicit Admin(const string& n = "Admin") : User(n) {}

    /**
     * @brief Returns this user's role label.
     * @return "Admin" (overrides User::getRole)
     */
    string getRole() const override { return "Admin"; }

    /**
     * @brief Checks if the supplied password is correct.
     * @param password The password to verify
     * @return true if password matches the admin credential
     */
    bool authenticate(const string& password) const {
        return password == "admin123";
    }
};

// ============================================================
// TEMPLATE FUNCTION: displayList<T>
//
// Prints a numbered list of any type T with operator<<.
// Used for both Question lists and string leaderboard lines,
// showing how one template works across different types.
//
// @tparam T     Item type (must have operator<< defined)
// @param items  Vector to display
// @param header Title above the list
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
// Parses questions.txt and returns all questions as a vector.
// This file is the bidirectional link with App 1 (main).
//
// @return Vector of Question objects from questions.txt
// ============================================================
vector<Question> loadQuestions() {
    vector<Question> questions;
    ifstream file("questions.txt");
    if (!file.is_open()) return questions;

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
// FUNCTION: saveQuestions
//
// Writes the entire question vector back to questions.txt.
// Called after deleting a question to persist the new state.
// App 1 (main) will see the updated file on its next run.
//
// @param questions  The updated question list to save
// ============================================================
void saveQuestions(const vector<Question>& questions) {
    ofstream file("questions.txt");
    for (const auto& q : questions) {
        file << q.text << "\n"
             << q.options[0] << "\n" << q.options[1] << "\n"
             << q.options[2] << "\n" << q.options[3] << "\n"
             << q.correct << "\n" << q.difficulty << "\n";
    }
}

// ============================================================
// COMMAND: view_questions
//
// Loads and displays all questions using displayList<Question>.
// ============================================================
void cmdViewQuestions() {
    vector<Question> questions = loadQuestions();
    displayList(questions, "QUESTION BANK");
    cout << "\nTotal: " << questions.size() << " question(s)\n";
}

// ============================================================
// COMMAND: add_question <text> <o1> <o2> <o3> <o4> <correct> <diff>
//
// Appends one new question to questions.txt (App 1 reads this).
// Validates that correct is 1-4 and difficulty is 1-3.
//
// @param text     Question text from argv
// @param opts     Four option strings from argv
// @param correct  Correct answer index (1-4)
// @param diff     Difficulty level (1-3)
// ============================================================
void cmdAddQuestion(const string& text, const string opts[4],
                    int correct, int diff) {
    if (correct < 1 || correct > 4) {
        cerr << "Error: correct answer must be 1-4. Got: " << correct << "\n";
        return;
    }
    if (diff < 1 || diff > 3) {
        cerr << "Error: difficulty must be 1-3. Got: " << diff << "\n";
        return;
    }

    ofstream file("questions.txt", ios::app);
    if (!file.is_open()) {
        cerr << "Error: could not open questions.txt\n";
        return;
    }

    file << text << "\n"
         << opts[0] << "\n" << opts[1] << "\n"
         << opts[2] << "\n" << opts[3] << "\n"
         << correct << "\n" << diff << "\n";

    cout << "Question added successfully.\n";
}

// ============================================================
// COMMAND: delete_question <number>
//
// Removes a question by its display number (1-based).
// Rewrites questions.txt via saveQuestions().
//
// @param number  1-based index of question to delete
// ============================================================
void cmdDeleteQuestion(int number) {
    vector<Question> questions = loadQuestions();

    if (number < 1 || number > (int)questions.size()) {
        cerr << "Error: question number " << number << " does not exist.\n";
        cerr << "Total questions: " << questions.size() << "\n";
        return;
    }

    questions.erase(questions.begin() + (number - 1));
    saveQuestions(questions);
    cout << "Question " << number << " deleted successfully.\n";
}

// ============================================================
// COMMAND: view_leaderboard
//
// Reads leaderboard.txt written by App 1 (main).
// This is the READ side of bidirectional file communication.
// ============================================================
void cmdViewLeaderboard() {
    ifstream file("leaderboard.txt");
    if (!file.is_open()) {
        cout << "Leaderboard is empty.\n";
        return;
    }

    vector<string> lines;
    string line;
    while (getline(file, line))
        if (!line.empty()) lines.push_back(line);

    displayList(lines, "LEADERBOARD");
}

// ============================================================
// COMMAND: delete_player <name>
//
// Removes all leaderboard entries for the named player.
// Reads the file, filters out matching entries, rewrites.
//
// NOTE: Uses a separate variable (targetName) to avoid the
// bug where the loop variable shadows the search variable.
//
// @param targetName  The player name to remove
// ============================================================
void cmdDeletePlayer(const string& targetName) {
    ifstream file("leaderboard.txt");
    if (!file.is_open()) {
        cerr << "Leaderboard file not found.\n";
        return;
    }

    vector<string> kept;
    string entryName;   // loop variable — separate from targetName
    int entryScore;
    bool found = false;

    while (file >> entryName >> entryScore) {
        if (entryName == targetName) {
            found = true;   // skip this entry (delete it)
        } else {
            kept.push_back(entryName + " " + to_string(entryScore));
        }
    }
    file.close();

    ofstream out("leaderboard.txt");
    for (const auto& l : kept) out << l << "\n";

    if (found) cout << "Player '" << targetName << "' removed.\n";
    else       cerr << "Player '" << targetName << "' not found.\n";
}

// ============================================================
// COMMAND: clear_leaderboard
//
// Empties leaderboard.txt completely.
// ============================================================
void cmdClearLeaderboard() {
    ofstream file("leaderboard.txt");
    cout << "Leaderboard cleared.\n";
}

// ============================================================
// COMMAND: view_progress
//
// Reads progress.txt written by App 1 and displays it.
// This is the READ side of bidirectional progress communication.
// ============================================================
void cmdViewProgress() {
    ifstream file("progress.txt");

    cout << "\n================================================\n";
    cout << "SAVED PROGRESS\n";
    cout << "================================================\n\n";

    if (!file.is_open()) {
        cout << "No progress file found.\n";
        return;
    }

    string name;
    int idx, score;

    if (file >> name >> idx >> score && name != "none") {
        cout << "Player: " << name << "\n";
        cout << "Next Question: " << idx + 1 << "\n";
        cout << "Score: " << score << "\n";
    } else {
        cout << "No active saved progress.\n";
    }
}

// ============================================================
// COMMAND: reset_progress
//
// Writes the empty sentinel to progress.txt.
// The player's next run of ./main play will start fresh.
// ============================================================
void cmdResetProgress() {
    ofstream file("progress.txt");
    if (file.is_open()) file << "none 0 0";
    cout << "Progress reset successfully.\n";
}

// ============================================================
// FUNCTION: printUsage
// Displays all available admin commands.
// ============================================================
void printUsage() {
    cout << "\n================================================\n";
    cout << "TRIVIA QUIZ SYSTEM - Admin App\n";
    cout << "================================================\n\n";
    cout << "  ./admin view_questions\n";
    cout << "  ./admin add_question <text> <opt1> <opt2> <opt3> <opt4> <correct(1-4)> <diff(1-3)>\n";
    cout << "  ./admin delete_question <number>\n";
    cout << "  ./admin view_leaderboard\n";
    cout << "  ./admin delete_player <name>\n";
    cout << "  ./admin clear_leaderboard\n";
    cout << "  ./admin view_progress\n";
    cout << "  ./admin reset_progress\n\n";
}

// ============================================================
// MAIN - Command-line argument dispatcher
//
// Uses a unique_ptr<Admin> to demonstrate smart pointer usage.
// Parses argv and routes to the correct command handler.
// Validates argument count and argument values before each call.
// No std::cin used anywhere.
// ============================================================
int main(int argc, char* argv[]) {
    // Smart pointer: unique_ptr manages Admin lifetime automatically
    unique_ptr<Admin> admin = make_unique<Admin>("AdminUser");

    if (argc < 2) {
        printUsage();
        return 0;
    }

    string command = argv[1];

    if (command == "view_questions") {
        cmdViewQuestions();

    } else if (command == "add_question") {
        if (argc < 9) {
            cerr << "Usage: ./admin add_question <text> <opt1> <opt2> <opt3> <opt4> <correct> <diff>\n";
            return 1;
        }
        string opts[4] = { argv[3], argv[4], argv[5], argv[6] };
        try {
            cmdAddQuestion(argv[2], opts, stoi(argv[7]), stoi(argv[8]));
        } catch (...) {
            cerr << "Error: correct and difficulty must be numbers.\n"; return 1;
        }

    } else if (command == "delete_question") {
        if (argc < 3) { cerr << "Usage: ./admin delete_question <number>\n"; return 1; }
        try {
            cmdDeleteQuestion(stoi(argv[2]));
        } catch (...) {
            cerr << "Error: question number must be a number.\n"; return 1;
        }

    } else if (command == "view_leaderboard") {
        cmdViewLeaderboard();

    } else if (command == "delete_player") {
        if (argc < 3) { cerr << "Usage: ./admin delete_player <name>\n"; return 1; }
        cmdDeletePlayer(argv[2]);

    } else if (command == "clear_leaderboard") {
        cmdClearLeaderboard();

    } else if (command == "view_progress") {
        cmdViewProgress();

    } else if (command == "reset_progress") {
        cmdResetProgress();

    } else {
        cerr << "Unknown command: '" << command << "'\n";
        printUsage();
        return 1;
    }

    return 0;
}
