#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>

// General format of each entry
struct Entry
{
  std::string service;
  std::string username;
  std::string password;
};

// ensure directories exist
bool ensureDirectoryExists(const std::string &path)
{
  if (std::filesystem::exists(path))
  {
    return true; // Already exists
  }

  std::cout << "The folder \"" << path << "\" does not exist. Create it? (y/n): ";
  char choice;
  std::cin >> choice;

  if (choice == 'y' || choice == 'Y')
  {
    try
    {
      std::filesystem::create_directory(path);
      std::cout << "Created: " << path << "\n";
      return true;
    }
    catch (const std::filesystem::filesystem_error &e)
    {
      std::cerr << "Error creating " << path << ": " << e.what() << "\n";
      return false;
    }
  }
  else
  {
    std::cout << "Skipping creation of: " << path << "\n";
    return false;
  }
}

// Check that the filesystem is currently mounted
bool isMounted(const std::string &mountPoint)
{
  std::ifstream mounts("/proc/mounts");
  std::string line;
  while (std::getline(mounts, line))
  {
    std::istringstream iss(line);
    std::string device, mount, type, opts;
    if (iss >> device >> mount >> type >> opts)
    {
      if (mount == mountPoint)
      {
        return true;
      }
    }
  }
  return false;
}

// Return true if mounted successfully, false otherwise
bool mountVault(const std::string &vaultCrypt, const std::string &vaultMount)
{
  if (isMounted(vaultMount))
  {
    return true;
  }

  pid_t pid = fork();
  if (pid == -1)
  {
    perror("fork");
    return false;
  }

  if (pid == 0) // Child process
  {
    // Build argument list (argv must be null-terminated)
    const char *args[] = {
        "gocryptfs",
        "-i", "10m",
        vaultCrypt.c_str(),
        vaultMount.c_str(),
        nullptr};

    // Replace process image
    execvp("gocryptfs", (char *const *)args);

    // If execvp returns, it failed
    perror("execvp");
    _exit(127);
  }
  else // Parent process
  {
    int status;
    if (waitpid(pid, &status, 0) == -1)
    {
      perror("waitpid");
      return false;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
      // Double-check the mount
      return isMounted(vaultMount);
    }
    else
    {
      std::cerr << "gocryptfs failed with status " << status << "\n";
      return false;
    }
  }
}

bool addEntry(const std::string &vaultMount)
{
  // Ask user for details
  Entry e;
  std::cout << "Service: ";
  std::getline(std::cin, e.service);
  std::cout << "Username: ";
  std::getline(std::cin, e.username);
  std::cout << "Password: ";
  std::getline(std::cin, e.password);

  // create a file in the vaultMount
  std::string filename = vaultMount + "/" + e.service;

  if (std::filesystem::exists(filename))
  {
    std::cerr << "Error: File \"" << filename << "\" already exists!\n";
    return false;
  }

  // Open the file inside the vault directory
  std::ofstream file(filename);

  if (!file)
  {
    std::cerr << "Error: Could not create file " << filename << "\n";
    return false;
  }
  // Write entry data
  file << e.service << "\n"
       << e.username << "\n"
       << e.password << "\n";

  file.close();

  std::cout << "\n---   File created: " << filename << "   ---\n";
  return true;
}

// get a vector of files contained in vault directory
std::vector<std::filesystem::directory_entry> getEntries(const std::string_view vaultMount)
{
  // create a vector to hold all files contained in the vault directory
  std::vector<std::filesystem::directory_entry> entries;

  // get files
  for (const auto &entry : std::filesystem::directory_iterator(vaultMount))
  {
    if (entry.is_regular_file())
    {
      entries.push_back(entry);
    }
  }

  return entries;
}

int chooseEntry(const std::vector<std::filesystem::directory_entry> &entries, const std::string &actionName)
{
  if (entries.empty())
  {
    std::cout << "No entries found in vault.\n";
    return -1;
  }

  // print numbered list
  std::cout << "\n=== Entries in vault ===\n";
  for (size_t i = 0; i < entries.size(); ++i)
  {
    std::cout << i + 1 << ") " << entries[i].path().filename().string() << "\n";
  }
  std::cout << "0) Exit\n";

  // get user choice
  int choice;
  std::cout << "Enter the number of the entry to " + actionName + ": ";
  std::cin >> choice;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  if (choice == 0)
  {
    return -1;
  }
  if (choice < 1 || choice > (int)entries.size())
  {
    std::cerr << "Invalid choice.\n";
    return -1;
  }

  return choice - 1;
}

// view files contained in vault directory
void viewEntries(const std::string_view vaultMount)
{
  // vector that contains all entries
  std::vector entries{getEntries(vaultMount)};
  int id = chooseEntry(entries, "view");
  if (id < 0)
  {
    std::cout << "Exiting.\n";
    return;
  }

  // open chosen file
  std::ifstream file(entries[id].path());
  if (!file)
  {
    std::cerr << "Error opening file.\n";
    return;
  }

  std::cout << "\n=== Content of " << entries[id].path().filename().string() << " ===\n";
  std::string line;
  while (std::getline(file, line))
  {
    std::cout << line << "\n";
  }
  std::cout << "========================\n";
}

bool editEntry(const std::string_view &vaultMount)
{
  // vector that contains all entries
  std::vector entries{getEntries(vaultMount)};
  int id = chooseEntry(entries, "edit");
  if (id < 0)
  {
    return false;
  }

  // open chosen file
  std::ifstream file(entries[id].path());
  if (!file)
  {
    std::cerr << "Error opening file.\n";
    return false;
  }

  // edit chosen file
  const std::filesystem::path &filePath = entries[id].path();

  Entry e;
  std::getline(file, e.service);
  std::getline(file, e.username);
  std::getline(file, e.password);

  file.close();

  std::cout << "\n=== Current entry ===\n";
  std::cout << "Service : " << e.service << "\n";
  std::cout << "Username: " << e.username << "\n";
  std::cout << "Password: " << e.password << "\n";
  std::cout << "=====================\n";

  // ask for edits (keep old value if left blank)
  std::string input;

  std::cout << "Enter new service (leave blank to keep current): ";
  std::getline(std::cin, input);
  if (!input.empty())
    e.service = input;

  std::cout << "Enter new username (leave blank to keep current): ";
  std::getline(std::cin, input);
  if (!input.empty())
    e.username = input;

  std::cout << "Enter new password (leave blank to keep current): ";
  std::getline(std::cin, input);
  if (!input.empty())
    e.password = input;

  // overwrite file with updated fields
  std::ofstream out(filePath, std::ios::trunc);
  if (!out)
  {
    std::cerr << "Error writing file.\n";
    return false;
  }
  out << e.service << "\n"
      << e.username << "\n"
      << e.password << "\n";
  out.close();

  // rename file
  std::string newFilename = e.service;

  if (!newFilename.empty() && newFilename != filePath.filename().string())
  {
    std::filesystem::path newPath = filePath.parent_path() / newFilename;
    try
    {
      std::filesystem::rename(filePath, newPath);
      std::cout << "File renamed to " << newFilename << "\n";
    }
    catch (const std::exception &e)
    {
      std::cerr << "Error renaming file: " << e.what() << "\n";
      return false;
    }
  }

  std::cout << "Entry updated successfully.\n";
  return true;
}

bool removeEntry(const std::string_view &vaultMount)
{
  // vector that contains all entries
  std::vector entries{getEntries(vaultMount)};
  int id = chooseEntry(entries, "remove");
  if (id < 0)
  {
    return false;
  }

  // open chosen file
  std::ifstream file(entries[id].path());
  if (!file)
  {
    std::cerr << "Error opening file.\n";
    return false;
  }

  // remove chosen file
  const std::filesystem::path &filePath = entries[id].path();
  try
  {
    if (std::filesystem::remove(filePath))
    {
      std::cout << "Removed: " << filePath.filename().string() << "\n";
      return true;
    }
    else
    {
      std::cerr << "Could not remove: " << filePath.filename().string() << "\n";
      return false;
    }
  }
  catch (const std::filesystem::filesystem_error &e)
  {
    std::cerr << "Filesystem error: " << e.what() << "\n";
    return false;
  }
};

void changePassword(const std::string_view &vaultCrypt)
{
  std::string cmd = "gocryptfs -passwd " + std::string(vaultCrypt);
  std::system(cmd.c_str());
}

int main()
{
  // get the users home directory
  std::string home{std::getenv("HOME")};
  // encrypted (where the encrypted files are stored)
  std::string vaultCrypt{home + "/.vault.crypt"};
  // plain (where the files are in plaintext)
  std::string vaultMount{home + "/vault"};

  // check that the folders exist
  if (!ensureDirectoryExists(vaultCrypt) || !ensureDirectoryExists(vaultMount))
  {
    std::cerr << "Vault setup incomplete. Exiting.\n";
    return 1;
  }

  // Possible choices
  int choice = -1;

  while (choice != 0)
  {
    // mount folder
    if (!mountVault(vaultCrypt, vaultMount))
    {
      std::cerr << "Vault mount failed. Exiting.\n";
      return 1;
    }

    std::cout << "\n=== Vault Menu ===\n"
              << "1) Add entry\n"
              << "2) View entries\n"
              << "3) Edit entry\n"
              << "4) Remove entry\n"
              << "5) Change password\n"
              << "0) Exit\n"
              << "\nSelect an option: ";

    if (!(std::cin >> choice))
    {
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "Invalid input. Try again.\n";
      continue;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    switch (choice)
    {
    case 1:
      if (!addEntry(vaultMount))
      {
        std::cerr << "Failed to create file. Exiting.\n";
        return 1;
      }
      break;
    case 2:
      viewEntries(vaultMount);
      break;
    case 3:
      if (!editEntry(vaultMount))
      {
        std::cerr << "Failed to edit file. Exiting.\n";
        return 1;
      }
      break;
    case 4:
      if (!removeEntry(vaultMount))
      {
        std::cerr << "Failed to remove file. Exiting.\n";
        return 1;
      }
      break;
      // Add case to edit an entry
    case 5:
      changePassword(vaultCrypt);
      break;
    case 0:
      std::cout << "Exiting...\n";
      break;
    default:
      std::cout << "Invalid option. Try again.\n";
    }
    std::cout << "Press Enter to continue\n";
    std::cin.get();
  }

  // unmount the vault before exiting
  std::string unmount = "fusermount -u " + std::string(vaultMount);
  std::system(unmount.c_str());
}