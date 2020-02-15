#ifndef SERVER_FRAMEWORK_CONFIG_H
#define SERVER_FRAMEWORK_CONFIG_H

#include "log.h"
#include "yaml-cpp/yaml.h"
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace zjl {

// @brief 配置项基类
class ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVarBase> ptr;

    ConfigVarBase(const std::string& name, const std::string& description)
        : m_name(name), m_description(description) {
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
    }
    virtual ~ConfigVarBase() = default;

    const std::string& getName() const { return m_name; }
    const std::string& getDesccription() const { return m_description; }
    // 将相的配置项的值转为为字符串
    virtual std::string toString() const = 0;
    // 通过字符串来获设置配置项的值
    virtual bool fromString(const std::string& val) = 0;

protected:
    std::string m_name;        // 配置项的名称
    std::string m_description; // 配置项的备注
};

/**
 * @brief 类型转换仿函数
 * boost::lexical_cast 的包装，
 * 因为 boost::lexical_cast 是使用 std::stringstream 实现的类型转换，
 * 所以仅支持实现了 ostream::operator<< 与 istream::operator>> 的类型,
 * 可以说默认情况下仅支持 std::string 与各类 Number 类型的双向转换。
 * 需要转换自定义的类型，可以选择实现对应类型的流操作符，或者将该模板类进行偏特化
*/
template <typename Source, typename Target>
class LexicalCast {
public:
    Target operator()(const Source& source) {
        return boost::lexical_cast<Target>(source);
    }
};

/**
 * @brief 类型转换仿函数
 * LexicalCast 的偏特化，针对 std::string 到 std::vector<T> 的转换，
 * 接受可被 YAML::Load() 解析的字符串
*/
template <typename T>
class LexicalCast<std::string, std::vector<T>> {
public:
    std::vector<T> operator()(const std::string& source) {
        YAML::Node node;
        // 调用 YAML::Load 解析传入的字符串，解析失败会抛出异常
        node = YAML::Load(source);
        std::vector<T> config_list;
        // 检查解析后的 node 是否是一个序列型 YAML::Node
        if (node.IsSequence()) {
            std::stringstream ss;
            for (const auto& item : node) {
                ss.str("");
                // 利用 YAML::Node 实现的 operator<<() 将 node 转换为字符串
                ss << item;
                // 递归解析，直到 T 为基本类型
                config_list.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
        } else {
            LOG_FMT_INFO(
                GET_ROOT_LOGGER(),
                "LexicalCast<std::string, std::vector>::operator() exception %s",
                "<source> is not a YAML sequence");
        }
        return config_list;
    }
};

/**
 * @brief 类型转换仿函数
 * LexicalCast 的偏特化，针对 std::list<T> 到 std::string 的转换，
*/
template <typename T>
class LexicalCast<std::vector<T>, std::string> {
public:
    std::string operator()(const std::vector<T>& source) {
        YAML::Node node;
        // 暴力解析，将 T 解析成字符串，在解析回 YAML::Node 插入 node 的尾部，
        // 最后通过 std::stringstream 与调用 yaml-cpp 库实现的 operator<<() 将 node 转换为字符串
        for (const auto& item : source) {
            // 调用 LexicalCast 递归解析，知道 T 为基本类型
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 类型转换仿函数
 * LexicalCast 的偏特化，针对 std::string 到 std::list<T> 的转换，
*/
template <typename T>
class LexicalCast<std::string, std::list<T>> {
public:
    std::list<T> operator()(const std::string& source) {
        YAML::Node node;
        node = YAML::Load(source);
        std::list<T> config_list;
        if (node.IsSequence()) {
            std::stringstream ss;
            for (const auto& item : node) {
                ss.str("");
                ss << item;
                config_list.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
        } else {
            LOG_FMT_INFO(
                GET_ROOT_LOGGER(),
                "LexicalCast<std::string, std::list>::operator() exception %s",
                "<source> is not a YAML sequence");
        }
        return config_list;
    }
};

/**
 * @brief 类型转换仿函数
 * LexicalCast 的偏特化，针对 std::list<T> 到 std::string 的转换，
*/
template <typename T>
class LexicalCast<std::list<T>, std::string> {
public:
    std::string operator()(const std::list<T>& source) {
        YAML::Node node;
        for (const auto& item : source) {
            node.push_back(YAML::Load(LexicalCast<T, std::string>()(item)));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

/**
 * @brief 通用型配置项的模板类
 * 模板参数:
 *      T               配置项的值的类型
 *      ToStringFN      {functor<std::string(T&)>} 将配置项的值转换为 std::string
 *      FromStringFN    {functor<T(const std::string&)>} 将 std::string 转换为配置项的值
 * */
template <
    class T,
    class ToStringFN = LexicalCast<T, std::string>,
    class FromStringFN = LexicalCast<std::string, T>>
class ConfigVar : public ConfigVarBase {
public:
    typedef std::shared_ptr<ConfigVar> ptr;

    ConfigVar(const std::string& name, const T& value, const std::string& description)
        : ConfigVarBase(name, description), m_value(value) {}

    const T getValue() const { return m_value; }
    void setValue(const T value) { m_value = value; }
    // 返回配置项的值的字符串
    std::string toString() const override {
        try {
            // 默认 ToStringFN 调用了 boost::lexical_cast 进行类型转换, 失败抛出异常 bad_lexical_cast
            return ToStringFN()(m_value);
        } catch (std::exception& e) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "ConfigVar::toString exception %s convert: %s to string",
                          e.what(),
                          typeid(m_value).name());
        }
        return "<error>";
    }
    // 将 yaml 文本转换为配置项的值
    bool fromString(const std::string& val) override {
        try {
            //  默认 FromStringFN 调用了 boost::lexical_cast 进行类型转换, 失败抛出异常 bad_lexical_cast
            m_value = FromStringFN()(val);
            return true;
        } catch (std::exception& e) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "ConfigVar::toString exception %s convert: string to %s",
                          e.what(),
                          typeid(m_value).name());
        }
        return false;
    }

private:
    T m_value; // 配置项的值
};

class Config {
public:
    typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;

    // 查找配置项，返回 ConfigVarBase 智能指针
    static ConfigVarBase::ptr
    Lookup(const std::string& name) {
        auto itor = s_data.find(name);
        if (itor == s_data.end()) {
            return nullptr;
        }
        return itor->second;
    }

    // 查找配置项，返回指定类型的 ConfigVar 智能指针
    template <class T>
    static typename ConfigVar<T>::ptr
    Lookup(const std::string& name) {
        /* 从 map 取出的配置项对象的类型为基类 ConfigVarBase，
         * 但我们需要使用其派生类的成员函数和变量，
         * 所以需要进行一次明确类型的显示转换
         */
        return std::dynamic_pointer_cast<ConfigVar<T>>(Lookup(name));
    }

    // 创建或更新配置项
    template <class T>
    static typename ConfigVar<T>::ptr
    Lookup(const std::string& name, const T& value, const std::string& description = "") {
        auto tmp = Lookup<T>(name);
        // 已存在同名配置项
        if (tmp) {
            LOG_FMT_INFO(GET_ROOT_LOGGER(),
                         "Config::Lookup name=%s 已存在",
                         name.c_str());
            return tmp;
        }
        // 判断名称是否合法
        if (name.find_first_not_of("qwertyuiopasdfghjklzxcvbnm0123456789._") != std::string::npos) {
            LOG_FMT_ERROR(GET_ROOT_LOGGER(),
                          "Congif::Lookup exception name=%s"
                          "参数只能以字母数字点或下划线开头",
                          name.c_str());
            throw std::invalid_argument(name);
        }
        auto v = std::make_shared<ConfigVar<T>>(name, value, description);
        s_data[name] = v;
        return v;
    }

    // 从 YAML::Node 中载入配置
    static void LoadFromYAML(const YAML::Node& root) {
        std::vector<std::pair<std::string, YAML::Node>> node_list;
        TraversalNode(root, "", node_list);
        // 遍历结果，更新 s_data
        std::stringstream ss;
        for (const auto& item : node_list) {
            auto itor = s_data.find(item.first);
            if (itor != s_data.end()) {
                ss.str("");
                ss << item.second;
                // 同名配置项则覆盖更新
                try {
                    s_data[item.first]->fromString(ss.str());
                } catch (const std::exception& e) {
                    LOG_FMT_ERROR(
                        GET_ROOT_LOGGER(),
                        "Config::LoadFromYAML exception what=%s",
                        e.what());
                }

            } else {
                // 约定大于配置原则，不对未约定的配置项进行解析
            }
        }
    }

private:
    // 遍历 YAML::Node 对象，并将遍历结果扁平化存到列表里返回
    static void TraversalNode(const YAML::Node& node, const std::string& name,
                              std::vector<std::pair<std::string, YAML::Node>>& output) {
        // 将 YAML::Node 存入 output
        auto itor = std::find_if(
            output.begin(),
            output.end(),
            [&name](const std::pair<std::string, YAML::Node> item) {
                return item.first == name;
            });
        if (itor != output.end()) {
            itor->second = node;
        } else {
            output.push_back(std::make_pair(name, node));
        }
        // 当 YAML::Node 为映射型节点，使用迭代器遍历
        if (node.IsMap()) {
            for (auto itor = node.begin(); itor != node.end(); ++itor) {
                TraversalNode(
                    itor->second,
                    name.empty() ? itor->first.Scalar()
                                 : name + "." + itor->first.Scalar(),
                    output);
            }
        }
        // 当 YAML::Node 为序列型节点，使用下标遍历
        if (node.IsSequence()) {
            for (size_t i = 0; i < node.size(); ++i) {
                TraversalNode(node[i], name + "." + std::to_string(i), output);
            }
        }
    }

private:
    static ConfigVarMap s_data;
};

/* util functional */
std::ostream& operator<<(std::ostream& out, const ConfigVarBase& cvb);
}

#endif
