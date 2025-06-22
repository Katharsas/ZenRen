#pragma once

#include <imgui.h>
#include <magic_enum.hpp>

namespace render::gui
{
	struct cstring {
		char* data = nullptr;

		cstring(const cstring&) = delete;
		cstring& operator=(const cstring&) = delete;

		cstring(cstring&& other)
			: data(nullptr)
		{
			*this = std::move(other);
		}

		cstring& operator=(cstring&& other)
		{
			if (this != &other)
			{
				delete[] data;
				data = other.data;
				other.data = nullptr;
			}
			return *this;
		}

		cstring() {};
		cstring(const std::string& str)
		{
			size_t len = str.length() + 1;
			data = new char[str.length() + 1];
			memcpy(data, str.c_str(), len);
		}
		template<int N>
		cstring(const char(&str)[N])
		{
			data = new char[N];
			memcpy(data, str, N);
		}
		~cstring()
		{
			if (data != nullptr) {
				delete[] data;
			}
		}
	};

	template <uint32_t N>
	struct ComboState {
		std::array<cstring, N> items;
		uint32_t selected;

		ComboState(std::array<cstring, N>&& items, uint32_t preselectedIndex = 0)
			: items(std::move(items)), selected(preselectedIndex) {}
	};

	template<typename T>
	ComboState<magic_enum::enum_count<T>()> comboStateFromEnum()
	{
		auto names = magic_enum::enum_names<T>();
		std::array<cstring, magic_enum::enum_count<T>()> items;

		for (uint32_t i = 0; i < items.size(); i++) {
			items[i] = cstring(std::string(names[i]));
		}
		return ComboState(std::move(items));
	}


	template <uint32_t N>
	void combo(const char* name, ComboState<N>& state, const std::function<void(uint32_t)>& select)
	{
		if (ImGui::BeginCombo(name, state.items[state.selected].data))
		{
			for (uint32_t n = 0; n < state.items.size(); n++) {
				const char* current = state.items[n].data;
				bool isSelected = (state.items[state.selected].data == current);
				if (ImGui::Selectable(current, isSelected)) {
					state.selected = n;
					select(n);
				}
			}
			ImGui::EndCombo();
		}
	}

	template <uint32_t N, typename T>
	void comboEnum(const char* name, ComboState<N>& state, T& target)
	{
		static_assert(N == magic_enum::enum_count<T>());
		combo(name, state, [&](uint32_t n) -> auto {
			target = static_cast<T>(n);
		});
	}
}
