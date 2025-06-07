#pragma once

namespace StringUtils
{
	inline std::vector<std::string> split(const std::string& a_str, const std::string& a_delimiter)
	{
		std::vector<std::string> result;
		size_t start = 0;
		size_t end = a_str.find(a_delimiter);

		while (end != std::string::npos) {
			result.push_back(a_str.substr(start, end - start));
			start = end + a_delimiter.length();
			end = a_str.find(a_delimiter, start);
		}

		result.push_back(a_str.substr(start));
		return result;
	}

	inline bool is_only_hex(std::string_view a_str, bool a_requirePrefix)
	{
		if (!a_requirePrefix) {
			return std::ranges::all_of(a_str, [](unsigned char ch) {
				return std::isxdigit(ch);
				});
		}
		else if (a_str.compare(0, 2, "0x") == 0 || a_str.compare(0, 2, "0X") == 0) {
			return a_str.size() > 2 && std::all_of(a_str.begin() + 2, a_str.end(), [](unsigned char ch) {
				return std::isxdigit(ch);
				});
		}
		return false;
	}

	inline std::string tolower(std::string_view a_str)
	{
		std::string result(a_str);
		std::ranges::transform(result, result.begin(), [](unsigned char ch) { return static_cast<unsigned char>(std::tolower(ch)); });
		return result;
	}

	template <class T>
	T to_num(const std::string& a_str, bool a_hex = false)
	{
		const int base = a_hex ? 16 : 10;

		if constexpr (std::is_same_v<T, double>) {
			return static_cast<T>(std::stod(a_str, nullptr));
		}
		else if constexpr (std::is_same_v<T, float>) {
			return static_cast<T>(std::stof(a_str, nullptr));
		}
		else if constexpr (std::is_same_v<T, std::int64_t>) {
			return static_cast<T>(std::stol(a_str, nullptr, base));
		}
		else if constexpr (std::is_same_v<T, std::uint64_t>) {
			return static_cast<T>(std::stoull(a_str, nullptr, base));
		}
		else if constexpr (std::is_signed_v<T>) {
			return static_cast<T>(std::stoi(a_str, nullptr, base));
		}
		else {
			return static_cast<T>(std::stoul(a_str, nullptr, base));
		}
	}

	template <typename T>
	T* GetFormFromString(const std::string& a_str)
	{
		T* response = nullptr;
		if (const auto splitID = split(a_str, "|"); splitID.size() == 2) {
			const auto& modName = splitID[0];
			if (!RE::TESDataHandler::GetSingleton()->LookupModByName(modName)) {
				return response;
			}
			if (!is_only_hex(splitID[1], true)) {
				return response;
			}

			try {
				const auto  formID = to_num<RE::FormID>(splitID[1], true);
				auto* intermediate = RE::TESDataHandler::GetSingleton()->LookupForm(formID, modName);
				if (intermediate) {
					return skyrim_cast<T*>(intermediate);
				}
			}
			catch (std::exception& e) {
				logger::error("Caught exception: {}", e.what());
				return response;
			}
		}
		auto* intermediate = RE::TESForm::LookupByEditorID(a_str);
		return intermediate ? skyrim_cast<T*>(intermediate) : nullptr;
	}
}