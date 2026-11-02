#include <iostream>
#include <string>

using namespace std;

namespace cas
{

    enum class SessionRole
    {
        Guest,
        Client,
        Admin
    };

    struct Session
    {
        SessionRole role = SessionRole::Guest;
        string accountId;
    };

    void clearSession(Session &session)
    {
        session.role = SessionRole::Guest;
        session.accountId.clear();
    }

    void showClientMenu()
    {
        cout << "\n=== Client Menu ===\n"
             << "1. Account summary\n"
             << "2. Deposit\n"
             << "3. Withdraw\n"
             << "4. Transfer\n"
             << "5. FX quote\n"
             << "6. Transaction history\n"
             << "7. Balance chart\n"
             << "8. Export statement or receipt\n"
             << "9. Logout\n";
    }

    void showAdminMenu()
    {
        cout << "\n=== Admin Menu ===\n"
             << "1. List accounts\n"
             << "2. Change account status\n"
             << "3. Read audit log\n"
             << "4. Logout\n";
    }

    void runClientMenu(Session &session)
    {
        string choice;

        while (true)
        {
            showClientMenu();
            cout << "Choose an option: ";

            if (!getline(cin, choice))
            {
                clearSession(session);
                return;
            }

            if (choice == "9")
            {
                clearSession(session);
                cout << "Logged out.\n";
                return;
            }

            if (choice.size() == 1 && choice >= "1" && choice <= "8")
            {
                cout << "This feature is being connected.\n";
            }
            else
            {
                cout << "Invalid choice. Please enter 1 to 9.\n";
            }
        }
    }

    void runAdminMenu(Session &session)
    {
        string choice;

        while (true)
        {
            showAdminMenu();
            cout << "Choose an option: ";

            if (!getline(cin, choice))
            {
                clearSession(session);
                return;
            }

            if (choice == "4")
            {
                clearSession(session);
                cout << "Logged out.\n";
                return;
            }

            if (choice.size() == 1 && choice >= "1" && choice <= "3")
            {
                cout << "This feature is being connected.\n";
            }
            else
            {
                cout << "Invalid choice. Please enter 1 to 4.\n";
            }
        }
    }

    void runMenu()
    {
        Session session;
        string choice;

        while (true)
        {
            if (session.role == SessionRole::Client)
            {
                runClientMenu(session);
                if (!cin)
                    return;
                continue;
            }

            if (session.role == SessionRole::Admin)
            {
                runAdminMenu(session);
                if (!cin)
                    return;
                continue;
            }

            cout << "\n=== Cents and Sensibility ===\n"
                 << "1. Login\n"
                 << "2. Register account\n"
                 << "3. About\n"
                 << "4. Exit\n"
                 << "Choose an option: ";

            if (!getline(cin, choice))
            {
                cout << "\nInput closed. Goodbye!\n";
                return;
            }

            if (choice == "1")
            {
                cout << "Login is coming next.\n";
            }
            else if (choice == "2")
            {
                cout << "Registration is coming next.\n";
            }
            else if (choice == "3")
            {
                cout << "CaS is a C++ banking simulation created by Group Ginitaan.\n";
            }
            else if (choice == "4")
            {
                cout << "Goodbye!\n";
                return;
            }
            else
            {
                cout << "Invalid choice. Please enter 1, 2, 3, or 4.\n";
            }
        }
    }

} // namespace cas