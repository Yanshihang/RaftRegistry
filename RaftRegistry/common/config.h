//
// File created on: 2024/03/12
// Author: Zizhou

#ifndef RR_CONFIG_H
#define RR_CONFIG_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>
#include <locale>
#include <yaml-cpp/yaml.h>
#include <libgo/libgo.h>

#include "util.h"


namespace RR {

    /**
     * @brief 配置变量类(ConfigVar)的基类
     * 
     */
class ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVarBase>;
    ConfigVarBase(std::string name, std::string description) : m_name(name), m_description(description) {}
    virtual ~ConfigVarBase() = default;

    std::string getName() const {
        return m_name;
    }

    std::string getDescription() const {
        return m_description;
    }

    virtual std::string getTypeName() const = 0;
    virtual std::string toString() = 0;
    virtual bool fromString(const std::string& value) = 0;
protected:
    std::string m_name;
    std::string m_description;
};

/**
 * @brief 重载调用运算符实现类型转换
 * 
 * @tparam To 目标类型
 * @tparam From 原类型
 */
template <typename To, typename From>
class LexicalCast {
public:
    To operator()(const From& value) {
        // 调用lexical_cast.h头文件中的的LexicalCastFunc函数，将From类型转换为To类型
        return LexicalCastFunc<To>(value);
    }
};

/**
 * @brief 将string类型的yaml配置存储到vector中
 * 
 * @tparam To vector中元素类型
 */
template <typename To>
class LexicalCast<std::vector<To>, std::string> {
public:
    std::vector<To> operator() (const std::string& from) {
        YAML::Node node = YAML::Load(from); // YAML::Load(str) 将输入的字符串 str 解析为一个 YAML::Node
        std::vector<To> res;
        std::stringstream ss;
        for ( auto i : node) {
            ss.str(""); // 清空ss
            ss << i; // YAML库定义了全局的<<yun运算符，可以将YAML::Node类型的数据输出到流中
            res.push_back(LexicalCast<To,std::string>()(ss.str()));
        }
    }
};

/**
 * @brief 将vector中存储的yaml配置转换为string类型
 * 
 * @tparam From vector中元素类型
 */
template <typename From>
class LexicalCast<std::string, std::vector<From>> {
public:
    std::string operator() (const std::vector<From>& from) {
        YAML::Node node;
        for ( auto& i : from) {
            node.push_back(YAML::Load(LexicalCast<std::string, From>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <typename To>
class LexicalCast<std::list<To>, std::string> {
public:
    std::list<To> operator() (const std::string& from) {
        YAML::Node node = YAML::Load(from);
        std::list<To> lis;
        std::stringstream ss;
        for ( auto i : node) {
            ss.str("");
            ss << i;
            lis.push_back(LexicalCast<To, std::string>()(ss.str()));
        }
        return lis;
    }
};

template <typename From>
class LexicalCast<std::string, std::list<From>> {
public:
    std::string operator() (const std::list<From>& from) {
        YAML::Node node;
        for (auto& i : from) {
            node.push_back(YAML::Load(LexicalCast<std::string, From>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <typename To>
class LexicalCast<std::set<To>, std::string> {
public:
    std::set<To> operator() (const string& from) {
        YAML::Node node = YAML::Load(from);
        std::set<To> res;
        std::stringstream ss;
        for (auto i : node) {
            ss.str("");
            ss << i;
            res.template emplace((LexicalCast<To, std::string>()(ss.str())));
        }
        return ss.str();
    }
};

template <typename From>
class LexicalCast<std::string, std::set<From>> {
public:
    std::string operator() (const std::set<From>& from) {
        YAML::Node node;
        for (auto& i : from) {
            node.push_back(YAML::Load(LexicalCast<std::string, From>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <typename To>
class LexicalCast<std::unordered_set<To>, std::string> {
public:
    std::unordered_set<To> operator() (const std::string& from) {
        YAML::Node node  = YAML::Load(from);
        std::unordered_set<To> res;
        std::stringstream ss;
        for (auto i : node) {
            ss.str("");
            ss << i;
            res.template emplace(LexicalCastTo, std::string>()(ss.str()));
        }
        return res;
    }
};

template <typename From>
class LexicalCast<std::string, std::unordered_set<From>> {
public:
    std::unordered_set<To> operator() (const std::unordered_set& from) {
        YAML::Node node;
        for ( auto& i : from) {
            node.push_back(YAML::Load(LexicalCast<std::string, From>()(i)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <typename To>
class LexicalCast<std::map<std::string, To>, std::string> {
public:
    std::map<std::string, To> operator() (const std::string& from) {
        YAML::Node node = YAML::Load(from);
        std::map<std::string, To> res;
        std::stringstream ss;
        for (auto i : node) {
            ss.str("");
            ss << i.second;
            // Scalar() 是 YAML::Node 的一个成员函数，它返回节点的标量值。如果节点是一个标量节点，那么 Scalar() 将返回该标量的字符串表示。
            // 如果节点不是一个标量节点，那么 Scalar() 的行为是未定义的。
            res.emplace(i.first.Scalar(), LexicalCast<To, std::string>()(ss.str()));
        }
        return res;
    }
};

template <typename From>
class LexicalCast<std::string, std::map<std::string, From>> {
public:
    std::string operator() (const std::map<std::string, From>& from) {
        YAML::Node node;
        for ( auto& i : from) {
            node[i.first] = YAML::Load(LexicalCast<std::string, From>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

template <typename To>
class LexicalCast<std::unordered_map<std::string, To>, std::string> {
public:
    std::unordered_map<std::string, To> operator() (const string& from) {
        YAML::Node node = YAML::Load(from);
        std::stringstream ss;
        std::unordered_map<std::string, To> res;
        for (auto i : node) {
            ss.str("");
            ss << i.second;
            res.emplace(i.first.Scalar(),LexicalCast<To, std::string>()(ss.str()));
        }
        return res;
    }
};

template <typename From>
class LexicalCast<std::string, std::unordered_map<std::string, From>> {
public:
    std::string operator() (const std::unordered_map<std::string, From>& from) {
        YAML::Node node;
        for (auto& i : from) {
            node[i.first] = YAML::Load(LexicalCast<std::string, From>()(i.second));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 定义一个配置变量类
 * 
 * @tparam T 配置项的类型
 * @tparam FromStr 可调用对象；默认值为LexicalCast<T, std::string>，将string类型转换为类型T
 * @tparam ToStr 可调用对象；默认值为LexicalCast<std::string, T>，将类型T转换为string类型
 * 
 * @note 虽然ConfigVar是ConfigVarBase的派生类，
 *       但是由于ConfigVar是一个模板类，所以使用不同的类型参数对ConfigVar模板类进行实例化时，每个实例化的模板类都会被视为一个独立的、不同的类型
 *       因此，使用不同类型参数实例化的派生类确实会导致不同的派生类类型。
 */
template <typename T, typename FromStr = LexicalCast<T, std::string>, typename ToStr = LexicalCast<std::string, T>>
class ConfigVar : public ConfigVarBase {
public:
    using ptr = std::shared_ptr<ConfigVar>;
    // 定义回调函数类型，用于在配置项改变时通知相关的监听者;
    // 回调函数的参数是当前值和改变之后的值
    using Callback = std::function<void(const T&,const T&)>;
    ConfigVar(const T& val,const std::string& name, const std::string& description) : ConfigVarBase(name, description), m_val(val) {}

    /**
     * @brief 获取当前配置项类型的名称
     * 
     * @return std::string 当前配置项类型的名称
     */
    std::string getTypeName() const override {
        return typeid(T).name();
    }

    /**
     * @brief 将当前配置项的值转换为字符串
     * 
     * @return std::string 当前配置项的值的字符串形式
     */
    std::string toString() override {
        try {
            std::unique_lock<co_rmutex> lock(m_mutex.Reader()); // 加上读锁
            return ToStr()(m_val);
        }catch (std::exception& e) {
            // GetLoggerInstance()是util中的函数，返回的是std::shared_ptr<spdlog::logger>类型的对象
            // 使用GetLoggerInstance()获取日志器，然后调用error()方法记录错误信息
            // 通过e.what()获取异常信息，然后通过typeid(T).name()获取T的类型信息
            GetLoggerInstance() -> template error("ConfigVar::toString() exception {} convert: {} toString", e.what(), typeid(T).name());
        }
        return "";
    }

    /**
     * @brief 将字符串转换为当前配置项的值
     * 
     * @param value 字符串类型的配置项的值
     * @return true 配置成功
     * @return false 配置失败
     */
    bool fromString(const std::string& value) override {
        try {
            setValue(FromStr()(value)); // 更新配置项的值
            return true;
        }catch (std::exception& e) {
            GetLoggerInstance() -> template error("ConfigVar::fromString() exception {} convert: {} fromString", e.what(), typeid(T).name());
        }
        return false;
    }

    /**
     * @brief 获取当前配置项的值
     * 
     * @return T 返回当前配置项的值
     */
    T getValue() {
        std::unique_lock<co_rmutex> lock(m_mutex.Reader());
        return m_val;
    }

    /**
     * @brief 更新配置项的值，如果更新成功，通知所有的监听者
     * 
     * @param value 要更新的配置项的值
     */
    void setValue(const T& value) {
        {
            std::unique_lock<co_rmutex> lock(m_mutex.Reader());
            if (value = m_val) {
                return;
            }
            for (auto& i : m_callbacks) {
                i.second(m_val,value);
            }
        }
        std::unique_lock<co_wmutex> lock(m_mutex.Writer());
        m_val = value;
    }

    /**
     * @brief 添加一个监听者函数
     * 
     * @param cb 监听者函数
     * @return uint64_t 返回添加的监听者函数的id
     */
    uint64_t addListener(Callback cb) {
        static uint64_t callbackId = 0; // 静态变量，使得每次调用addListener()时，cbId都是唯一的
        std::unique_lock<co_wmutex> lock(m_mutex.Writer());
        ++callbackId;
        m_callbacks[callbackId] = cb;
        return callbackId;
    }

    /**
     * @brief 根据给定的callbackId删除相应的监听者函数
     * 
     * @param callbackId 
     */
    void deleteListener(uint64_t callbackId) {
        std::unique_lock<co_wmutex> lock(m_mutex.Writer());
        m_callbacks.erase(callbackId);
    }

    /**
     * @brief 清楚所有监听者函数
     */
    void clearListener() {
        std::unique_lock<co_wmutex> lock(m_mutex.Writer());
        m_callbacks.clear();
    }

    /**
     * @brief 根据给定的callbackId获取相应的监听者函数
     * 
     * @param callbackId 想要获取的监听者函数的id
     * @return Callback 监听者函数
     */
    Callback getListener(uint64_t callbackId) {
        std::unique_lock<co_rmutex> lock(m_mutex.Reader());

        // 不能直接返回m_callbacks[callbackId]，因为如果callbackId不存在，会自动创建一个新的元素
        // return m_callbacks[callbackId];

        auto iter = m_callbacks.find(callbackId);
        if (iter == m_callbacks.end()) {
            return nullptr;
        }else {
            return iter -> second;
        }
    }

private:
    T m_val; // 当前配置项的值
    std::map<int64_t, Callback> m_callbacks; // 用于存储回调函数
    co_rwmutex m_mutex; // 读写锁，用于线程安全的访问和修改配置项（m_val)的值(也包括m_callbacks的值)
};

/**
 * @brief 用于管理配置项
 * @details 允许通过字符串键来查询和定义配置项，并能够从文件或YAML格式的数据中加载配置。
 *          此外，还支持对配置项进行遍历。
 */
class Config {
public:
    // 使用map来存储配置变量，键为配置项名称，值为指向配置类的智能指针(静态类型为配置类的基类)
    // 这里之所以使用ConfigVarBase::ptr类型的指针，是因为ConfigVarBase是ConfigVar的基类
    // 通过ConfigVarBase::ptr类型的指针，可以指向ConfigVar的对象
    // 不直接使用ConfigVar::ptr类型的指针，是因为ConfigVar是一个模板类，使用不同的类型参数实例化的派生类确实会导致不同的派生类类型
    using configVarMap = std::map<std::string, ConfigVarBase::ptr>;

    /**
     * @brief 查找指定名称的配置变量（ConfigVar的对象），如果没有则创建一个新的配置变量，并加入到映射中。
     * 
     * @tparam T ConfigVar中配置项的类型
     * @param name 配置项的名称
     * @return ConfigVar<T>::ptr 
     * 
     * @details 实现两个功能：
     *          查询: 它尝试在配置项映射中查找给定名称的配置变量。
     *          创建与添加: 如果给定名称的配置变量不存在，它将创建一个新的配置变量并添加到映射中。
     */
    template <typename T>
    static typename ConfigVar<T>::ptr LookUp(const std::string& name) {
        // 因为要查询并返回一个ConfigVar<T>::ptr类型的对象，所以需要加上读锁
        std::unique_lock<co_rmutex> lock(GetMutex().Reader());
        auto iter = GetDatas().find(name);

        // 如果name不存在，返回nullptr
        if (iter == GetDatas().end()) {
            return nullptr;
        }else {
            return std::dynamic_pointer_cast<ConfigVar<T>::ptr>(iter -> second);
        }
    }

    template <typename T>
    static typename ConfigVar<T>::ptr LookUp(const std::string& name, const T& value, const std::string& description) {
        std::unique_lock<co_wmutex> lock(GetMutex().Writer());
        auto iter = GetDatas().find(name);

        // 如果name已经存在，返回该对象的智能指针
        if (iter != GetDatas().end()) {
            // 通过dynamic_pointer_cast将ConfigVarBase::ptr类型的对象转换为ConfigVar<T>::ptr类型的对象
            // 如果返回的是nullptr，说明name已经存在，但是类型不匹配，记录错误信息，并返回nullptr
            auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(iter -> second);
            // 如果name已经存在，但是类型不匹配，记录错误信息，并返回nullptr
            if (tmp) {
                SPDLOG_LOGGER_INFO(GetLoggerInstance(),"lookup name={} already exist", name);
                return tmp;
            }else {
                SPDLOG_LOGGER_INFO(GetLoggerInstance(),"lookup name={} already exist but type not {}, exact type is {}, value is {}", name, typeid(T).name(), iter->second->getTypeName(), iter->second->toString());
                return nullptr;
            }
        }
        // 如果name中有非法字符，记录错误信息，并throw一个异常
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._") != std::string::npos) {
            SPDLOG_LOGGER_ERROR(GetLoggerInstance(),"lookup invalid name = {}",name);
            throw std::invalid_argument(name);
        }

        // 实现创建与添加功能
        // 如果没找到name，且name的命名合法，就创建一个新的ConfigVar<T>对象，并加入到映射中
        auto v = std::make_shared(ConfigVar<T>(value,name,description));
        GetDatas()[name] = v;
        return v;
    }

    /**
     * @brief 实现一个与特定类型无关的方式来查询配置变量
     * 
     * @param name 
     * @return ConfigVarBase::ptr 
     * 
     * @details 已有两个lookup函数的情况下，这个函数的作用是什么？
     *          类型中立: LookupBase允许你查询一个配置变量而不需要在调用时指定它的具体类型。这在你只需要基类ConfigVarBase提供的接口，而不关心具体的配置变量类型时非常有用，例如，当你只想检查一个配置变量是否存在，或者遍历所有的配置变量进行一些通用操作时。
     *          减少依赖: 使用LookupBase可以避免在某些情况下对模板类型参数的依赖，使得代码更简洁、更清晰。
     *          多态性: LookupBase允许以多态的方式处理不同类型的配置变量。通过返回基类指针ConfigVarBase::ptr，你可以在运行时根据需要对其进行动态类型转换，使用动态绑定的方法来调用派生类的实现。
     */
    static ConfigVarBase::ptr LookUpBase(const std::string& name);

    // 从文件中加载配置项
    static void LoadFromFile(const std::string& path);

    /**
     * @brief 更新现有配置变量的值
     * 
     * @details 函数的逻辑主要是遍历YAML文件的所有节点，并尝试更新已经存在于ConfigMap中的配置变量的值;
     *          而不是直接将配置项添加到ConfigMap中
     */
    static void LoadFromYaml(const YAML::Node& node);

    // 访问所有配置变量
    static void Visit(std::function<void(ConfigVarBase::ptr)> cb);
private:
    // 返回包含着所有配置项的map
    static configVarMap& GetDatas() {
        static configVarMap p_datas;
        return p_datas;
    }

    // 返回读写锁
    static co_rwmutex& GetMutex() {
        static co_rwmutex p_mutex;
        return p_mutex;
    }
};

}


#endif //RR_CONFIG_H