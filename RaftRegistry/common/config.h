//
// File created on: 2024/03/12
// Author: Zizhou

#ifndef RR_CONFIG_H
#define RR_CONFIG_H

#include <string>



namespace RR {
class ConfigVarBase {
public:
    ConfigVarBase(std::string name, std::string description) : m_name(name), m_description(description) {}
    virtual ~ConfigVarBase() = default;

    std::string getName() const {
        return m_name;
    }

    std::string getDescription() const {
        return m_description;
    }

    virtual std::string getType() const = 0;
    virtual std::string toString() const = 0;
    virtual bool fromString(const std::string& value) = 0;
protected:
    std::string m_name;
    std::string m_description;
};


}


#endif //RR_CONFIG_H