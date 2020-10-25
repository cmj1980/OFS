#include "KeybindingSystem.h"

#include "OpenFunscripter.h"

#include "imgui.h"

#include <algorithm>

void KeybindingSystem::setup()
{
    auto app = OpenFunscripter::ptr;
    app->events->Subscribe(SDL_KEYDOWN, EVENT_SYSTEM_BIND(this, &KeybindingSystem::KeyPressed));
    app->events->Subscribe(SDL_CONTROLLERBUTTONUP, EVENT_SYSTEM_BIND(this, &KeybindingSystem::ControllerButtonUp));
    app->events->Subscribe(SDL_CONTROLLERBUTTONDOWN, EVENT_SYSTEM_BIND(this, &KeybindingSystem::ControllerButtonDown));
}

void KeybindingSystem::KeyPressed(SDL_Event& ev) noexcept
{
    auto key = ev.key;
    auto modstate = key.keysym.mod;
    
    std::array<uint16_t, 3> possibleModifiers{
        KMOD_SHIFT,
        KMOD_CTRL,
        KMOD_ALT
    };
    // filter out modifiers which shouldn't affect bindings
    modstate &= ~(KMOD_NUM | KMOD_CAPS | KMOD_GUI | KMOD_MODE);

    // convert KMOD_LSHIFT / KMOD_RSHIFT -> KMOD_SHIFT etc.
    uint16_t genericModifiers = 0;
    for (auto possibleModifier : possibleModifiers) {
        if (modstate & possibleModifier)
            genericModifiers |= possibleModifier;
    }
    modstate = genericModifiers;

    if (currentlyChanging != nullptr) {
        if (key.repeat) return;
        if (key.keysym.sym == SDLK_ESCAPE) {
            currentlyChanging = nullptr;
            return;
        }
        currentlyHeldKeys.str("");
        
        if(modstate & KMOD_CTRL) {
            addKeyString("Ctrl");
            switch (key.keysym.sym) {
            case SDLK_LCTRL:
            case SDLK_RCTRL:
                return;
            }
        }

        if (modstate & KMOD_ALT) {
            addKeyString("Alt");
            switch (key.keysym.sym) {
            case SDLK_LALT:
            case SDLK_RALT:
                return;
            }
        }

        if (modstate & KMOD_SHIFT) {
            addKeyString("Shift");
            switch (key.keysym.sym) {
            case SDLK_LSHIFT:
            case SDLK_RSHIFT:
            return;
            }
        }
        addKeyString(SDL_GetKeyName(key.keysym.sym));

        currentlyChanging->key.key = key.keysym.sym;
        currentlyChanging->key.key_str = currentlyHeldKeys.str();
        binding_string_cache[currentlyChanging->identifier] = currentlyChanging->key.key_str;


        currentlyChanging->key.modifiers = modstate;
        currentlyChanging = nullptr;
        return;
    }
    if (ShowWindow) return;

    // this prevents keybindings from being processed when typing into a textbox etc.
    if (ImGui::IsAnyItemActive()) return;
    // process bindings
    for (auto& group : ActiveBindings) {
        for (auto& binding : group.bindings) {
            if (key.repeat && binding.key.ignore_repeats) continue;

            if (key.keysym.sym == binding.key.key) {
                bool modifierMismatch = false;
                for (auto possibleModifier : possibleModifiers) {
                    if ((modstate & possibleModifier) != (binding.key.modifiers & possibleModifier)) {
                        modifierMismatch = true;
                        break;
                    }
                }
                if (modifierMismatch) continue;
            }
            else {
                continue;
            }

            // execute binding
            binding.action(0);
            break;
        }
    }
}

void KeybindingSystem::addKeyString(const char* name)
{
    if (currentlyHeldKeys.tellp() == 0)
        currentlyHeldKeys << name;
    else
        currentlyHeldKeys << "+" << name;
}

void KeybindingSystem::addKeyString(char name)
{
    if (currentlyHeldKeys.tellp() == 0)
        currentlyHeldKeys << name;
    else
        currentlyHeldKeys << "+" << name;
}

void KeybindingSystem::ControllerButtonDown(SDL_Event& ev) noexcept
{
}

void KeybindingSystem::ControllerButtonUp(SDL_Event& ev) noexcept
{
}

std::string KeybindingSystem::loadKeyString(SDL_Keycode key, int mod)
{
    currentlyHeldKeys.str("");
    if (mod & KMOD_CTRL) {
        addKeyString("Ctrl");
    }

    if (mod & KMOD_ALT) {
        addKeyString("Alt");
    }

    if (mod & KMOD_SHIFT) {
        addKeyString("Shift");
    }
    addKeyString(SDL_GetKeyName(key));

    return currentlyHeldKeys.str();
}

const std::string& KeybindingSystem::getBindingString(const char* binding_id) noexcept
{
    auto it = binding_string_cache.find(binding_id);
    if (it != binding_string_cache.end())
        return binding_string_cache[binding_id];
    static std::string empty("", 0);
    return empty;
}

void KeybindingSystem::setBindings(const std::vector<KeybindingGroup>& groups)
{
    // setBindings only does something if the bindings were previously registered
    for (auto& group : groups) {
        for (auto& keybind : group.bindings) {
            auto groupIt = std::find_if(ActiveBindings.begin(), ActiveBindings.end(),
                [&](auto& activeGroup) { return activeGroup.name == group.name; });

            if (groupIt != ActiveBindings.end()) {
                auto it = std::find_if(groupIt->bindings.begin(), groupIt->bindings.end(),
                    [&](auto& active) { return active.identifier == keybind.identifier; });

                if (it != groupIt->bindings.end()) {
                    // override defaults
                    it->key.ignore_repeats = keybind.key.ignore_repeats;
                    it->key.key = keybind.key.key;
                    it->key.modifiers = keybind.key.modifiers;
                    it->key.key_str = loadKeyString(keybind.key.key, keybind.key.modifiers);
                    binding_string_cache[it->identifier] = it->key.key_str;
                }
            }
        }
    }
}

void KeybindingSystem::registerBinding(const KeybindingGroup& group)
{
    ActiveBindings.emplace_back(std::move(group));
    for (auto& binding : ActiveBindings.back().bindings) {
        binding.key.key_str = loadKeyString(binding.key.key, binding.key.modifiers);
        binding_string_cache[binding.identifier] = binding.key.key_str;
    }
}

bool KeybindingSystem::ShowBindingWindow()
{
    bool save = false;
    if (ShowWindow)
        ImGui::OpenPopup("Keys");

    if (ImGui::BeginPopupModal("Keys", &ShowWindow, ImGuiWindowFlags_None | ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        constexpr bool disable_indent = true;
        if constexpr (disable_indent)
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 0.0f);

        ImGui::Text("You can use CTRL, SHIFT & ALT as modifiers.");
        ImGui::Text("There's no checking for duplicate bindings. So just don't do that.");
        ImGui::Text("The keybindings get saved everytime a change is made.");
        ImGui::Text("Config: \"data/keybindings.json\"\nIf you wan't to revert to defaults delete the config.");


        ImGui::Separator();
        int id = 0;
        for(auto& group : ActiveBindings)
        {
            ImGui::Columns(1);
            if (ImGui::CollapsingHeader(group.name.c_str())) {
                ImGui::PushID(group.name.c_str());
                
                ImGui::Columns(3, "bindings");
                ImGui::Separator();
                ImGui::Text("Action"); ImGui::NextColumn();
                ImGui::Text("Current"); ImGui::NextColumn();
                ImGui::Text("Ignore repeats"); ImGui::NextColumn();
                for (auto& binding : group.bindings) {
                    ImGui::PushID(id++);
                    ImGui::Text("%s", binding.description.c_str()); ImGui::NextColumn();
                    if(ImGui::Button(!binding.key.key_str.empty() ? binding.key.key_str.c_str() : "-- Not set --", ImVec2(-1.f, 0.f))) {
                        currentlyChanging = &binding;
                        currentlyHeldKeys.str("");
                        ImGui::OpenPopup("Change Binding");
                    }
                    if (ImGui::Button("Controller", ImVec2(-1.f, 0.f))) {

                    }
                    ImGui::NextColumn();
                    if (ImGui::Checkbox("", &binding.key.ignore_repeats)) { save = true; } ImGui::NextColumn();

                    if (ImGui::BeginPopupModal("Change Binding", 0, ImGuiWindowFlags_AlwaysAutoResize)) 
                    {
                        if (currentlyHeldKeys.tellp() == 0)
                            ImGui::Text("Press any key...\nEscape to cancel.");
                        else
                            ImGui::Text(currentlyHeldKeys.str().c_str());
                        if (!currentlyChanging) {
                            save = true; // autosave
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
                ImGui::PopID();
            }
        }
        ImGui::Columns(1);
        ImGui::Separator();

        /*save = ImGui::Button("Save", ImVec2(-1, 0));*/

        if constexpr (disable_indent)
            ImGui::PopStyleVar();

        ImGui::EndPopup();
    }
    return save;
}
