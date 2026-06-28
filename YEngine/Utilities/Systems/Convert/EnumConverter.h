#include <string>
#include <unordered_map>
#include <initializer_list>

template <typename T>
class YEnumConverter {
private:
    std::unordered_map<T, std::string> toStr;
    std::unordered_map<std::string, T> toEnum;
    T defaultValue;

public:
    // コンストラクタでリストを渡すだけで双方向マップを自動生成
    YEnumConverter(T def, std::initializer_list<std::pair<T, std::string>> list)
        : defaultValue(def)
    {
        for (const auto& pair : list) {
            toStr[pair.first] = pair.second;
            toEnum[pair.second] = pair.first;
        }
    }

    // Enum -> String
    std::string ToString(T value) const {
        auto it = toStr.find(value);
        return (it != toStr.end()) ? it->second : toStr.at(defaultValue);
    }

    // String -> Enum
    T ToEnum(const std::string& str) const {
        auto it = toEnum.find(str);
        return (it != toEnum.end()) ? it->second : defaultValue;
    }
};