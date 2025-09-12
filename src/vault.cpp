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

// view files contained in vault directory
void viewEntries(const std::string_view vaultMount)
{
  // vector that contains all entries
  std::vector entries{getEntries(vaultMount)};

  if (entries.empty())
  {
    std::cout << "No entries found in vault.\n";
    return;
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
  std::cout << "Enter the number of the entry to view: ";
  std::cin >> choice;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  if (choice == 0)
  {
    std::cout << "Exiting view.\n";
    return;
  }
  if (choice < 1 || choice > (int)entries.size())
  {
    std::cerr << "Invalid choice.\n";
    return;
  }

  // open chosen file
  std::ifstream file(entries[choice - 1].path());
  if (!file)
  {
    std::cerr << "Error opening file.\n";
    return;
  }

  std::cout << "\n=== Content of " << entries[choice - 1].path().filename().string() << " ===\n";
  std::string line;
  while (std::getline(file, line))
  {
    std::cout << line << "\n";
  }
  std::cout << "========================\n";
}

bool removeEntry(const std::string_view &vaultMount)
{
  // vector that contains all entries
  std::vector entries{getEntries(vaultMount)};

  if (entries.empty())
  {
    std::cout << "No entries found in vault.\n";
    return false;
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
  std::cout << "Enter the number of the entry to remove: ";
  std::cin >> choice;
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  if (choice == 0)
  {
    std::cout << "Exiting view.\n";
    return true;
  }
  if (choice < 1 || choice > (int)entries.size())
  {
    std::cerr << "Invalid choice.\n";
    return false;
  }

  // open chosen file
  std::ifstream file(entries[choice - 1].path());
  if (!file)
  {
    std::cerr << "Error opening file.\n";
    return false;
  }

  // remove chosen file
  const std::filesystem::path &filePath = entries[choice - 1].path();
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
              << "3) Remove entry\n"
              << "4) Change password\n"
              << "0) Exit\n"
              << "Select an option: ";

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
      if (!removeEntry(vaultMount))
      {
        std::cerr << "Failed to remove file. Exiting.\n";
        return 1;
      }
      break;
      // Add case to edit an entry
    case 4:
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