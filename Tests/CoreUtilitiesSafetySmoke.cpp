#include "RawIron/Core/ActionBindings.h"
#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Core/InputLabelFormat.h"

#include <cstdlib>
#include <string>
#include <vector>

int main() {
    using ri::core::ActionBinding;
    using ri::core::ActionBindings;
    using ri::core::BindingSlot;

    if (ri::core::NormalizeKeyboardInputId(" keyw ") != "KeyW"
        || ri::core::NormalizeKeyboardInputId("DIGIT7") != "Digit7"
        || ri::core::NormalizeKeyboardInputId("f24") != "F24"
        || ri::core::NormalizeKeyboardInputId("f25") == "F25"
        || ri::core::NormalizeKeyboardInputId("mouse4") != "MouseBack"
        || ri::core::NormalizeKeyboardInputId("return") != "Enter"
        || ri::core::NormalizeKeyboardInputId("CTRLRIGHT") != "ControlRight"
        || ri::core::NormalizeKeyboardInputId("arrowup") != "ArrowUp"
        || ri::core::FormatInputLabelFromInputId("mouse5") != "Mouse 5"
        || ri::core::FormatInputLabelFromInputId("foo__bar--baz") != "Foo Bar Baz"
        || ri::core::FormatNormalizedKeyboardLabel("DigitA") == "A") {
        return EXIT_FAILURE;
    }

    ActionBindings bindings;
    if (!bindings.DefineAction(ActionBinding{
            .actionId = "  Jump  ",
            .displayName = "   ",
            .primaryInputId = "keyw",
            .secondaryInputId = "KEYW",
        })) {
        return EXIT_FAILURE;
    }
    const ActionBinding* jump = bindings.FindAction(" Jump ");
    if (jump == nullptr || jump->actionId != "Jump" || jump->displayName != "Jump"
        || jump->primaryInputId != "KeyW" || !jump->secondaryInputId.empty()) {
        return EXIT_FAILURE;
    }
    if (bindings.DefineAction(ActionBinding{.actionId = "Fire", .primaryInputId = "KEYW"})
        || bindings.DefineAction(ActionBinding{.actionId = "   "})) {
        return EXIT_FAILURE;
    }
    if (!bindings.DefineAction(ActionBinding{
            .actionId = "Crouch",
            .primaryInputId = "KeyC",
            .secondaryInputId = "KeyV",
        })
        || !bindings.Rebind("Crouch", "keyv", BindingSlot::Primary, false)) {
        return EXIT_FAILURE;
    }
    const ActionBinding* crouch = bindings.FindAction("Crouch");
    if (crouch == nullptr || crouch->primaryInputId != "KeyV" || !crouch->secondaryInputId.empty()
        || bindings.ResolveAction("KEYV").value_or("") != "Crouch"
        || bindings.Rebind("Crouch", "KeyX", static_cast<BindingSlot>(99), true)
        || bindings.ClearBinding("Crouch", static_cast<BindingSlot>(99))) {
        return EXIT_FAILURE;
    }
    if (!bindings.DefineAction(ActionBinding{
            .actionId = "Locked",
            .primaryInputId = "KeyL",
            .allowRebind = false,
        })
        || bindings.Rebind("Locked", "KeyK")
        || bindings.ClearBinding("Locked", BindingSlot::Primary)) {
        return EXIT_FAILURE;
    }

    using namespace ri::core::detail;
    if (ParseQuotedString("\"bad\\q\"", 0U).has_value()
        || ParseQuotedString("\"\\uD800\"", 0U).has_value()
        || ParseQuotedString(std::string("\"bad\nvalue\""), 0U).has_value()) {
        return EXIT_FAILURE;
    }
    const std::optional<std::string> unicode = ParseQuotedString("\"A\\u0042\\uD83D\\uDE00\"", 0U);
    if (!unicode.has_value() || *unicode != std::string("AB\xF0\x9F\x98\x80")) {
        return EXIT_FAILURE;
    }
    if (ExtractJsonString("{\"name\":\"value\"junk}", "name").has_value()
        || ExtractJsonDouble("{\"value\":1e999}", "value").has_value()) {
        return EXIT_FAILURE;
    }
    const std::vector<std::string> validArray = ExtractJsonStringArray("{\"items\":[\"a\",\"b\"]}", "items");
    if (validArray != std::vector<std::string>{"a", "b"}
        || !ExtractJsonStringArray("{\"items\":[\"a\" \"b\"]}", "items").empty()
        || !ExtractJsonStringArray("{\"items\":[\"a\",]}", "items").empty()
        || !ExtractJsonStringArray("{\"items\":[\"a\"}", "items").empty()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
