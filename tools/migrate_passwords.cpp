// Password migration utility
// Converts plaintext passwords in users.yml to hashed passwords
// Usage: migrate_passwords.exe config/users.yml config/users.yml.new

#include "../src/core/password_hash.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_users.yml> <output_users.yml>" << std::endl;
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    
    try {
        YAML::Node root = YAML::LoadFile(inputFile);
        
        if (!root["users"]) {
            std::cerr << "Error: No 'users' section found in " << inputFile << std::endl;
            return 1;
        }
        
        int migrated = 0;
        int skipped = 0;
        
        auto users = root["users"];
        for (auto it = users.begin(); it != users.end(); ++it) {
            std::string username = it->first.as<std::string>();
            YAML::Node userNode = it->second;
            
            if (userNode["password"]) {
                std::string password = userNode["password"].as<std::string>();
                
                // Check if already hashed
                if (PasswordHash::isHashed(password)) {
                    std::cout << "Skipping " << username << " (already hashed)" << std::endl;
                    skipped++;
                } else {
                    // Hash the password
                    std::string hashed = PasswordHash::hash(password);
                    userNode["password"] = hashed;
                    std::cout << "Migrated " << username << std::endl;
                    migrated++;
                }
            }
        }
        
        // Write output file
        std::ofstream out(outputFile);
        out << root;
        out.close();
        
        std::cout << "\nMigration complete:" << std::endl;
        std::cout << "  Migrated: " << migrated << " users" << std::endl;
        std::cout << "  Skipped: " << skipped << " users (already hashed)" << std::endl;
        std::cout << "\nOutput written to: " << outputFile << std::endl;
        std::cout << "Review the file and replace the original when ready." << std::endl;
        
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}

