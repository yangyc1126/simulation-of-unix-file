#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_NAME 64
#define MAX_INPUT 128
#define MAX_PATH_LEN 1024

typedef struct Node {
    char name[MAX_NAME];
    bool isDirectory;
    struct Node* parent;
    struct Node* child;
    struct Node* sibling;
} Node;

Node* root;
Node* cwd;
bool verbose = false;

// Trim leading and trailing whitespace from a string
char* trim(char* str) {
    if (!str) return str;
    char* start = str;
    while (*start && isspace(*start)) start++;
    char* end = start + strlen(start);
    while (end > start && isspace(*(end - 1))) end--;
    *end = '\0';
    return start;
}

// Normalize name by removing extra slashes and handling paths
void normalizeName(char* name) {
    if (!name) return;
    char temp[MAX_NAME];
    int j = 0;
    bool lastWasSlash = false;

    // Special case for root "/"
    if (strlen(name) == 1 && name[0] == '/') {
        temp[j++] = '/';
        temp[j] = '\0';
        strncpy(name, temp, MAX_NAME - 1);
        name[MAX_NAME - 1] = '\0';
        return;
    }

    // Skip leading slashes
    int i = 0;
    while (name[i] == '/') i++;
    for (; name[i] && j < MAX_NAME - 1; i++) {
        if (name[i] == '/' && lastWasSlash) continue;
        lastWasSlash = (name[i] == '/');
        temp[j++] = name[i];
    }
    // Remove trailing slashes
    while (j > 0 && temp[j - 1] == '/') j--;
    temp[j] = '\0';
    strncpy(name, temp, MAX_NAME - 1);
    name[MAX_NAME - 1] = '\0';
}

Node* createNode(const char* name, bool isDirectory) {
    char normalizedName[MAX_NAME];
    strncpy(normalizedName, name, MAX_NAME - 1);
    normalizedName[MAX_NAME - 1] = '\0';
    normalizeName(normalizedName);
    if (strlen(normalizedName) == 0 && strcmp(name, "/") != 0) {
        printf("Invalid name: %s (empty or too long after normalization)\n", name);
        return NULL;
    }
    if (strlen(normalizedName) >= MAX_NAME) {
        printf("Invalid name: %s (too long after normalization)\n", name);
        return NULL;
    }
    Node* node = (Node*)malloc(sizeof(Node));
    if (!node) {
        printf("Memory allocation failed.\n");
        return NULL;
    }
    strncpy(node->name, normalizedName, MAX_NAME - 1);
    node->name[MAX_NAME - 1] = '\0';
    node->isDirectory = isDirectory;
    node->parent = NULL;
    node->child = NULL;
    node->sibling = NULL;
    return node;
}

void insertChild(Node* parent, Node* child) {
    if (!parent || !child) return;
    if (!parent->child) {
        parent->child = child;
    } else {
        Node* temp = parent->child;
        while (temp->sibling) temp = temp->sibling;
        temp->sibling = child;
    }
    child->parent = parent;
    if (verbose) printf("Inserted %s as child of %s\n", child->name, parent->name);
}

Node* findChild(Node* parent, const char* name) {
    if (!parent || !name) {
        if (verbose) printf("findChild: Parent or name is NULL\n");
        return NULL;
    }
    if (verbose) printf("findChild: Looking for %s in children of %s\n", name, parent->name);
    Node* temp = parent->child;
    while (temp) {
        if (verbose) printf("findChild: Checking child %s\n", temp->name);
        if (strcmp(temp->name, name) == 0) {
            if (verbose) printf("findChild: Found %s\n", name);
            return temp;
        }
        temp = temp->sibling;
    }
    if (verbose) printf("findChild: %s not found\n", name);
    return NULL;
}

void freeTree(Node* node) {
    if (!node) return;
    freeTree(node->child);
    freeTree(node->sibling);
    free(node);
}

void pwd(Node* node, bool inlinePrompt) {
    if (!node) {
        printf("Error: Current directory is NULL.\n");
        return;
    }
    char path[MAX_PATH_LEN];
    int depth = 0;
    Node* current = node;
    char* segments[MAX_PATH_LEN];
    while (current) {
        segments[depth++] = current->name;
        current = current->parent;
    }
    if (depth == 0) {
        printf("Error: Invalid path.\n");
        return;
    }
    int pathLen = 0;
    if (depth == 1 && strcmp(segments[0], "/") == 0) {
        path[pathLen++] = '/';
    } else {
        for (int i = depth - 1; i >= 0; i--) {
            if (i == depth - 1 && strcmp(segments[i], "/") == 0) continue;
            path[pathLen++] = '/';
            strncpy(path + pathLen, segments[i], MAX_PATH_LEN - pathLen - 1);
            pathLen += strlen(segments[i]);
        }
    }
    path[pathLen] = '\0';
    if (inlinePrompt) {
        printf("%s$ ", pathLen == 0 ? "/" : path);
    } else {
        if (verbose) printf("Current directory: ");
        printf("%s\n", pathLen == 0 ? "/" : path);
    }
    fflush(stdout);
}

void printTreeRecursive(Node* node, const char* prefix, int isLast) {
    if (!node) return;
    printf("%s", prefix);
    if (node != root) {
        printf("%s── %s%s\n", isLast ? "└" : "├", node->name, node->isDirectory ? "/" : "");
    } else {
        printf(".\n");
    }

    char newPrefix[1024];
    snprintf(newPrefix, sizeof(newPrefix), "%s%s   ", prefix, isLast ? "    " : "│");

    Node* child = node->child;
    int count = 0;
    for (Node* temp = child; temp; temp = temp->sibling) count++;

    int i = 0;
    while (child) {
        printTreeRecursive(child, newPrefix, i == count - 1);
        child = child->sibling;
        i++;
    }
}

void printTree(Node* start) {
    if (!start) {
        printf("Error: Tree is empty.\n");
        return;
    }
    if (verbose) printf("Displaying file system tree:\n");
    printTreeRecursive(start, "", 1);
}

Node* findNodeFromPath(Node* start, const char* path) {
    if (!path || strlen(path) == 0) return start;
    Node* target = start;
    char pathCopy[MAX_INPUT];
    strncpy(pathCopy, path, MAX_INPUT - 1);
    pathCopy[MAX_INPUT - 1] = '\0';
    char* token = strtok(pathCopy, "/");
    while (token) {
        if (strlen(token) == 0) {
            token = strtok(NULL, "/");
            continue;
        }
        Node* next = findChild(target, token);
        if (next && next->isDirectory) {
            target = next;
        } else {
            return NULL;
        }
        token = strtok(NULL, "/");
    }
    return target;
}

void mkdir(const char* name) {
    if (!name || strlen(name) == 0) {
        printf("Error: Directory name is empty.\n");
        return;
    }
    if (findChild(cwd, name)) {
        printf("Directory already exists.\n");
        return;
    }
    Node* newDir = createNode(name, true);
    if (newDir) {
        insertChild(cwd, newDir);
        if (verbose) printf("Created directory: %s\n", name);
    }
}

void createFile(const char* name) {
    if (!name || strlen(name) == 0) {
        printf("Error: File name is empty.\n");
        return;
    }
    if (findChild(cwd, name)) {
        printf("File already exists.\n");
        return;
    }
    Node* newFile = createNode(name, false);
    if (newFile) {
        insertChild(cwd, newFile);
        if (verbose) printf("Created file: %s\n", name);
    }
}

void rmdir(const char* name) {
    if (!name || strlen(name) == 0) {
        printf("Error: Directory name is empty.\n");
        return;
    }
    if (strcmp(name, "/") == 0) {
        printf("Error: Cannot remove root directory.\n");
        return;
    }
    Node* dir = findChild(cwd, name);
    if (!dir) {
        printf("No such directory.\n");
        return;
    }
    if (!dir->isDirectory) {
        printf("Error: %s is not a directory.\n", name);
        return;
    }
    if (dir->child) {
        printf("Error: Directory %s is not empty.\n", name);
        return;
    }
    if (cwd == dir) {
        printf("Error: Cannot remove current working directory.\n");
        return;
    }
    // Unlink the directory
    if (cwd->child == dir) {
        cwd->child = dir->sibling;
    } else {
        Node* prev = cwd->child;
        while (prev && prev->sibling != dir) prev = prev->sibling;
        if (prev) prev->sibling = dir->sibling;
    }
    if (verbose) printf("Removed directory: %s\n", name);
    free(dir);
}

void rm(const char* name) {
    if (!name || strlen(name) == 0) {
        printf("Error: File name is empty.\n");
        return;
    }
    Node* file = findChild(cwd, name);
    if (!file) {
        printf("No such file.\n");
        return;
    }
    if (file->isDirectory) {
        printf("Error: %s is a directory.\n", name);
        return;
    }
    // Unlink the file
    if (cwd->child == file) {
        cwd->child = file->sibling;
    } else {
        Node* prev = cwd->child;
        while (prev && prev->sibling != file) prev = prev->sibling;
        if (prev) prev->sibling = file->sibling;
    }
    if (verbose) printf("Removed file: %s\n", name);
    free(file);
}

void ls() {
    if (!cwd) {
        printf("Error: Current directory is NULL.\n");
        return;
    }
    Node* temp = cwd->child;
    if (!temp && verbose) {
        printf("Directory is empty.\n");
        return;
    }
    if (verbose && temp) printf("Listing contents of current directory:\n");
    while (temp) {
        printf("%s%s\n", temp->name, temp->isDirectory ? "/" : "");
        temp = temp->sibling;
    }
}

void cd(const char* name) {
    if (!name || strlen(name) == 0) {
        if (verbose && cwd != root) printf("Changed to root directory\n");
        cwd = root;
        return;
    }
    if (strcmp(name, "..") == 0) {
        if (cwd->parent) {
            if (verbose) printf("Changed to parent directory\n");
            cwd = cwd->parent;
        } else if (verbose) {
            printf("Already at root directory\n");
        }
        return;
    }
    // Handle absolute paths starting with '/'
    Node* target = cwd;
    if (name[0] == '/') {
        target = root;
        name++; // Skip the leading '/'
    }
    // Split path by '/' for absolute or relative paths
    char path[MAX_INPUT];
    strncpy(path, name, MAX_INPUT - 1);
    path[MAX_INPUT - 1] = '\0';
    char* token = strtok(path, "/");
    while (token) {
        if (strlen(token) == 0) {
            token = strtok(NULL, "/");
            continue;
        }
        Node* next = findChild(target, token);
        if (next && next->isDirectory) {
            target = next;
            if (verbose) printf("Changed to directory: %s\n", token);
        } else {
            printf("No such directory: %s.\n", token);
            return;
        }
        token = strtok(NULL, "/");
    }
    cwd = target;
}

void save(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Filename is empty.\n");
        return;
    }
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Error: Could not open file %s.\n", filename);
        return;
    }
    void saveNode(Node* node, FILE* file, int depth) {
        if (!node) return;
        for (int i = 0; i < depth; i++) fprintf(file, "  ");
        fprintf(file, "%s %d\n", node->name, node->isDirectory ? 1 : 0);
        saveNode(node->child, file, depth + 1);
        saveNode(node->sibling, file, depth);
    }
    saveNode(root, file, 0);
    fclose(file);
    if (verbose) printf("Saved file system to: %s\n", filename);
    else printf("File system saved to %s.\n", filename);
}

void reload(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Filename is empty.\n");
        return;
    }
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: Could not open file %s.\n", filename);
        return;
    }
    // Free the old tree
    freeTree(root);
    root = NULL;
    cwd = NULL;

    char line[MAX_INPUT];
    Node* stack[MAX_PATH_LEN];
    int stackTop = -1;
    int lineNumber = 0;

    // Process all lines
    while (fgets(line, sizeof(line), file)) {
        lineNumber++;
        char* trimmedLine = trim(line);
        if (strlen(trimmedLine) == 0) continue;
        if (verbose) printf("reload: Processing line %d: '%s'\n", lineNumber, trimmedLine);

        int depth = 0;
        char* ptr = line;
        while (*ptr == ' ') {
            depth++;
            ptr++;
        }
        if (depth % 2 != 0) {
            printf("Error at line %d: Invalid indentation: '%s'\n", lineNumber, trimmedLine);
            fclose(file);
            freeTree(root);
            root = createNode("/", true);
            cwd = root;
            return;
        }

        char name[MAX_NAME];
        int isDir;
        if (sscanf(trimmedLine, "%s %d", name, &isDir) != 2) {
            printf("Error at line %d: Invalid line format: '%s'\n", lineNumber, trimmedLine);
            continue;
        }

        char normalizedName[MAX_NAME];
        strncpy(normalizedName, name, MAX_NAME - 1);
        normalizedName[MAX_NAME - 1] = '\0';
        normalizeName(normalizedName);
        if (strlen(normalizedName) == 0) {
            printf("Error at line %d: Invalid name (empty after normalization)\n", lineNumber);
            continue;
        }

        int currentLevel = depth / 2;
        if (verbose) printf("reload: Depth=%d, currentLevel=%d, stackTop=%d\n", depth, currentLevel, stackTop);

        // Initialize root if not set
        if (!root) {
            if (!isDir) {
                printf("Error at line %d: First entry must be a directory: '%s'\n", lineNumber, trimmedLine);
                fclose(file);
                root = createNode("/", true);
                cwd = root;
                return;
            }
            root = createNode(normalizedName, true);
            if (!root) {
                fclose(file);
                printf("Error: Failed to create new root.\n");
                return;
            }
            cwd = root;
            stack[++stackTop] = root;
            if (verbose) printf("reload: Set new root to %s (stackTop=%d)\n", normalizedName, stackTop);
            continue;
        }

        // Adjust stack to the parent level
        while (stackTop >= 0 && stackTop >= currentLevel) {
            stackTop--;
            if (verbose) printf("reload: Popped stack, stackTop=%d\n", stackTop);
        }
        stackTop++; // Move to the current level
        if (stackTop >= MAX_PATH_LEN) {
            printf("Error at line %d: Stack overflow.\n", lineNumber);
            fclose(file);
            freeTree(root);
            root = createNode("/", true);
            cwd = root;
            return;
        }

        Node* newNode = createNode(normalizedName, isDir);
        if (!newNode) {
            fclose(file);
            freeTree(root);
            root = createNode("/", true);
            cwd = root;
            return;
        }
        if (verbose) printf("reload: Adding %s (isDir=%d) at level %d, parent=%s\n", normalizedName, isDir, currentLevel, stack[stackTop-1]->name);
        insertChild(stack[stackTop-1], newNode);
        stack[stackTop] = newNode;
    }

    if (!root) {
        root = createNode("/", true);
        cwd = root;
        if (verbose) printf("reload: No valid entries found, using default /\n");
        else printf("File system reloaded from %s.\n", filename);
    } else {
        if (verbose) printf("Reloaded file system from: %s\n", filename);
        else printf("File system reloaded from %s.\n", filename);
    }
    fclose(file);
}

void rmsave(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Filename is empty.\n");
        return;
    }
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Error: File %s does not exist.\n", filename);
        return;
    }
    fclose(file);
    if (remove(filename) == 0) {
        if (verbose) printf("Removed saved file: %s\n", filename);
        else printf("File %s removed.\n", filename);
    } else {
        printf("Error: Could not remove file %s.\n", filename);
    }
}

bool askToSave() {
    char response[MAX_INPUT];
    printf("Would you like to save the file system before exiting? (y/n): ");
    fflush(stdout);
    if (!fgets(response, sizeof(response), stdin)) {
        printf("Error: Invalid input. Exiting without saving.\n");
        return false;
    }
    response[strcspn(response, "\n")] = 0;
    if (strlen(response) == 0) {
        printf("Error: Empty input. Exiting without saving.\n");
        return false;
    }
    char c = tolower(response[0]);
    if (c == 'y') {
        printf("Enter filename to save: ");
        fflush(stdout);
        char filename[MAX_INPUT];
        if (!fgets(filename, sizeof(filename), stdin)) {
            printf("Error: Invalid filename. Exiting without saving.\n");
            return false;
        }
        filename[strcspn(filename, "\n")] = 0;
        if (strlen(filename) == 0) {
            printf("Error: Empty filename. Exiting without saving.\n");
            return false;
        }
        save(filename);
        return true;
    } else if (c == 'n') {
        if (verbose) printf("Exiting without saving.\n");
        return false;
    } else {
        printf("Error: Invalid input. Please enter 'y' or 'n'.\n");
        return askToSave();
    }
}

void setVerbose(const char* arg) {
    if (!arg || strlen(arg) == 0) {
        printf("Error: Specify 'on' or 'off'.\n");
        return;
    }
    if (strcmp(arg, "on") == 0) {
        verbose = true;
        printf("Verbose mode enabled.\n");
    } else if (strcmp(arg, "off") == 0) {
        verbose = false;
        printf("Verbose mode disabled.\n");
    } else {
        printf("Error: Invalid argument. Use 'on' or 'off'.\n");
    }
}

void showPrompt() {
    if (!cwd) {
        printf("Error: Current directory is NULL.\n");
        return;
    }
    pwd(cwd, true);
}

void printMenu() {
    printf("menu\n        print out all commands\n");
    printf("verbose [on|off]\n        turn on/off verbose mode\n");
    printf("mkdir pathname\n        create an empty directory\n");
    printf("rmdir pathname\n        remove an empty directory\n");
    printf("cd [pathname]\n        change directory\n");
    printf("ls\n        list files and directories in the working directory\n");
    printf("tree [pathname]\n        print out the file system tree from the specified path or current directory\n");
    printf("pwd\n        print working directory\n");
    printf("create pathname\n        create a file\n");
    printf("rm pathname\n        remove a file\n");
    printf("save [pathname]\n        save the file system structure into a file\n");
    printf("reload [pathname]\n        reload the file system structure from a file\n");
    printf("rmsave [pathname]\n        remove a saved file system file\n");
    printf("quit\n        exit the program (prompts to save file system)\n");
}

void executeCommand(const char* cmdLine) {
    if (!cmdLine || strlen(cmdLine) == 0) return;
    char cmd[MAX_INPUT] = "";
    char arg[MAX_INPUT] = "";
    sscanf(cmdLine, "%s %[^\n]", cmd, arg);

    if (verbose && strcmp(cmd, "verbose") != 0) {
        printf("Executing command: %s %s\n", cmd, arg);
    }

    if (strcmp(cmd, "menu") == 0) printMenu();
    else if (strcmp(cmd, "verbose") == 0) setVerbose(arg);
    else if (strcmp(cmd, "pwd") == 0) {
        pwd(cwd, false);
    } else if (strcmp(cmd, "mkdir") == 0) mkdir(arg);
    else if (strcmp(cmd, "rmdir") == 0) rmdir(arg);
    else if (strcmp(cmd, "create") == 0) createFile(arg);
    else if (strcmp(cmd, "rm") == 0) rm(arg);
    else if (strcmp(cmd, "ls") == 0) ls();
    else if (strcmp(cmd, "cd") == 0) cd(arg);
    else if (strcmp(cmd, "tree") == 0) {
        if (strlen(arg) == 0) {
            printTree(cwd);
        } else {
            Node* start = findNodeFromPath(root, arg);
            if (start) {
                printTree(start);
            } else {
                printf("No such directory: %s.\n", arg);
            }
        }
    } else if (strcmp(cmd, "save") == 0) save(arg);
    else if (strcmp(cmd, "reload") == 0) reload(arg);
    else if (strcmp(cmd, "rmsave") == 0) rmsave(arg);
    else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
        if (verbose) printf("Preparing to exit.\n");
        askToSave();
        if (verbose) printf("Exiting program.\n");
        freeTree(root);
        exit(0);
    } else {
        printf("Unknown command: %s\n", cmd);
    }
}

int main() {
    root = createNode("/", true);
    if (!root) {
        printf("Failed to initialize file system.\n");
        return 1;
    }
    cwd = root;

    char input[MAX_INPUT];
    while (1) {
        showPrompt();
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0;
        if (strlen(input) == 0) continue;
        executeCommand(input);
    }

    freeTree(root);
    return 0;
}