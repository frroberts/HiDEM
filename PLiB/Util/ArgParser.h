//
// Created by Fredrik Robertsen on 23/02/2021.
//

#ifndef PHELL_ARGPARSER_H
#define PHELL_ARGPARSER_H

#include <unordered_map>
#include <variant>
#include <iomanip>

#include <toml.hpp>
#include "TomlParser.h"
#include "../Logger/Logger.h"
#include "Util.h"
#include "../Geometry/Map2dLoader.h"

using namespace std::literals;

namespace Parsers {

    template<typename T>
    inline
    T getAs(std::string value) {
        return T();
    }

    template<>
    inline
    std::string getAs<std::string>(std::string value) {
        return value;
    }

    template<>
    inline
    double getAs<double>(std::string value) {
        return std::stof(value);
    }

    template<>
    inline
    float getAs<float>(std::string value) {
        return std::stof(value);
    }

    template<>
    inline
    int getAs<int>(std::string value) {
        return std::stoi(value);
    }

    template<>
    inline
    bool getAs<bool>(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (value == "true" || value == "1")
            return true;
        return false;
    }

    template<typename T>
    std::vector<T> getAsArr(std::string value) {
        std::vector<T> ret;

        // drop first and last []
        value.erase(0, 1);
        value.erase(value.size() - 1, 1);

        // std::cout << value << std::endl;

        std::string del = ",";
        int start = 0;
        int end = value.find(del);
        while (end != -1) {
            ret.push_back(getAs<T>(value.substr(start, end - start)));
            start = end + del.size();
            end = value.find(del, start);
        }
        ret.push_back(getAs<T>(value.substr(start, end - start)));

        return ret;
    }

    /*
    template<>
    std::vector<std::string> getAsArr<std::string>(std::string value) {
        std::vector<std::string> ret;
        return ret;
    }

    template<>
    std::vector<double> getAsArr<double>(std::string value) {
    std::vector<double> ret;
        return ret;//std::stof(value);
    }

    template<>
    std::vector<float> getAsArr<float>(std::string value) {
    std::vector<float> ret;
        return ret;//std::stof(value);
    }

    template<>
    std::vector<int> getAsArr<int>(std::string value) {
    std::vector<int> ret;
        return ret;//std::stoi(value);
    }

    template<>
    std::vector<bool> getAsArr<bool>(std::string value) {
    std::vector<bool> ret;
    return ret;
    }
     */
};

class ArgParser {
    std::unordered_map<std::string, std::string> lineAarguments;
    std::vector<std::string> cliArgs;
    bool printHelp = false;
    std::string configPath = "";

    std::string stripPrefix(std::string val, char prefix) {
        //
        while (val.front() == prefix) {
            val = val.substr(1);
        }
        return val;
    }

    template<class T>
    std::string printVec(std::vector<T> vec) {
        std::string ret = "";
        ret += "[" + std::to_string(vec.at(0));
        if (vec.size() > 1) {
            for (auto e = vec.begin() + 1; e != vec.end(); e++) {
                ret += "," + std::to_string(*e);
            }
        }
        ret += "]";
        return ret;
    }

    std::string printVec(std::vector<std::string> vec) {
        std::string ret = "";
        ret += "["s + "\""s + vec.at(0) + "\""s;
        if (vec.size() > 1) {
            for (auto e = vec.begin() + 1; e != vec.end(); e++) {
                ret += "," + "\""s + *e + "\""s;
            }
        }
        ret += "]";
        return ret;
    }



public:
    toml::value table;

    ArgParser(int argc, char *argv[]) {
        //
        if (argc == 1) {
            // print usage
            printHelp = true;
        }
        for (int i = 1; i < argc; ++i) {
            std::string argument = argv[i];
            while (i < argc - 2 && std::string_view(argv[i + 1]).front() != '-') {
                i++;
                argument += std::string(" ") + std::string(argv[i]);
            }
            auto spacePlace = argument.find(" ");
            auto equalPlace = argument.find("=");
            auto splitSpace = std::min(spacePlace, equalPlace);
            if (splitSpace != std::string::npos) {
                lineAarguments[stripPrefix(argument.substr(0, splitSpace), '-')] = argument.substr(splitSpace + 1);
            } else {
                lineAarguments[argument] = "true";
            }
        }

        if (!printHelp) {
            configPath = argv[argc - 1];
            table = toml::parse(configPath);
        }
    }

    std::string getConfigPath() {
        return configPath;
    };


    template<class T>
    std::optional<T> getArg(std::string name) {
        std::optional<T> lineValue;
        std::optional<T> ret;
        cliArgs.push_back(name);

        //auto tomlValue = config->get_qualified_as<T>(name);
        std::optional<T> tomlValue;
        if (auto pos = name.find("."); pos == std::string::npos) {
            //tomlValue = tomlTable[name].value<T>(); //config->get_qualified_as<T>(name);
             TomlParser::getVal<T>(name, table, tomlValue);// toml::find<T>(table, name);
        } else {
            //
            std::string first = name.substr(0, pos);
            std::string second = name.substr(pos + 1, name.length());

            if (table.contains(first)) {
                toml::value tomlFirst = toml::find(table, first);
                if (tomlFirst.contains(second))
                     TomlParser::getVal<T>(second, tomlFirst, tomlValue);//toml::find<T>(tomlFirst, second);
                //tomlValue = tomlTable[first][second].value<T>(); //config->get_qualified_as<T>(name);
            }
        }
        auto linePos = lineAarguments.find(name);
        if (linePos != lineAarguments.end()) {
            lineValue = std::make_optional(Parsers::getAs<T>(linePos->second));
        }
        if (!lineValue && !tomlValue) {
            if (!printHelp)
                Logger::writeAtFirst(name + " not found, setting to default \n");
        } else if (tomlValue && !lineValue) {
            //std::cout << name << " toml and not line" << std::endl;
            ret = std::make_optional(*tomlValue);
        } else if (lineValue) {
            // push line value into toml table
            // todo fix for new toml lib
            if (auto pos = name.find("."); pos == std::string::npos) {
                //tomlTable.insert_or_assign(name, lineValue.value());
                //TomlParser::setVal(name, table, lineValue.value());
                table[name] = lineValue.value();

            } else {
                //
                std::string first = name.substr(0, pos);
                std::string second = name.substr(pos + 1, name.length());
                table[first][second] = lineValue.value();

            }
            //std::cout << name << " line val" << std::endl;
            ret = lineValue;
        }
        return ret;
    }

    // gets a pointer to 2d map from the config file
    template<class T>
    std::optional<Map2d<T>*> getMap2dPtr(std::string name) {
                bool found = false;
        toml::value tomlMap;
        if (auto pos = name.find("."); pos == std::string::npos) {
            tomlMap = toml::find(table, name);
            found = true;
        } else {
            //
            std::string first = name.substr(0, pos);
            std::string second = name.substr(pos + 1, name.length());

            if (table.contains(first)) {
                toml::value tomlFirst = toml::find(table, first);
                if (tomlFirst.contains(second)) {
                    tomlMap = toml::find(tomlFirst, second);
                    found = true;
                }
            }
        }

        // figure out what map we are building
        if (found && tomlMap.is_table()) {
            // todo check what type it is, current is only csv2d
            // todo clean up memory
            Map2dLoader loader;
            loader.readInput(toml::find<std::string>(tomlMap, "path"));
            auto ptr = loader.get<lpReal_t>(toml::find<int>(tomlMap, "fieldID"));
            return ptr;
        }
        return {};
    }

    void printPossibleCLIArgs() {
        if (printHelp) {
            if (getLocalRank() == 0)
                for (auto s: cliArgs) {
                    std::cout << "--" << s << "=" << std::endl;
                }
            exit(0);
        }
    }

};


#endif //PHELL_ARGPARSER_H
