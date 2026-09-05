#include "RawIron/Core/ActionBindings.h"
#include "RawIron/Core/Detail/JsonScan.h"
#include "RawIron/Core/InputLabelFormat.h"
#include "RawIron/Math/Mat4.h"

#include <cstdlib>
#include <limits>
#include <string>
#include <vector>
#include <cstdio>

bool TestNormalTransforms() {
    using namespace ri::math;
    const Vec3 tangentA{1,0,1}, tangentB{0,1,1};
    const Vec3 normal=Normalize(Cross(tangentA,tangentB));
    for (const Vec3 scale : {Vec3{2,3,.5f},Vec3{-2,3,.5f},Vec3{1.e-20f,2.e-20f,3.e-20f},Vec3{1.e20f,2.e20f,3.e20f}}) {
        const auto matrix=TRS({17,22,13},{21,37,13},scale);
        const auto actual=TransformNormal(matrix,normal);
        const float magnitude=std::abs(scale.x);
        const auto relativeScale=scale/magnitude;
        // Analytic inverse scale then rotation, independent of cofactor implementation.
        const auto expected=Normalize(TransformVector(RotationXYZDegrees({21,37,13}),
            Normalize({normal.x/relativeScale.x,normal.y/relativeScale.y,normal.z/relativeScale.z})));
        if (!std::isfinite(actual.x) || std::abs(Length(actual)-1)>1.e-5f) return false;
        if (Distance(actual,expected)>1.e-5f) return false;
        const auto shape=TRS({}, {21,37,13},relativeScale);
        if (std::abs(Dot(actual,Normalize(TransformVector(shape,tangentA))))>1.e-5f
            || std::abs(Dot(actual,Normalize(TransformVector(shape,tangentB))))>1.e-5f) return false;
    }
    auto shear=IdentityMatrix(); shear.m[0][1]=.75f; shear.m[2][0]=-.3f;
    const auto sheared=TransformNormal(shear,normal);
    if (std::abs(Dot(sheared,TransformVector(shear,tangentA)))>1.e-5f
        || std::abs(Dot(sheared,TransformVector(shear,tangentB)))>1.e-5f) return false;
    auto invalid=IdentityMatrix(); invalid.m[0][0]=std::numeric_limits<float>::infinity();
    return LengthSquared(TransformNormal(ScaleMatrix({0,1,1}),normal))==0
        && LengthSquared(TransformNormal(invalid,normal))==0
        && LengthSquared(TransformNormal(IdentityMatrix(),{}))==0
        && LengthSquared(TransformNormal(IdentityMatrix(),{std::numeric_limits<float>::quiet_NaN(),0,1}))==0;
}

int main() {
    if (!TestNormalTransforms()) { std::fprintf(stderr,"Affine normal transform regression\n"); return EXIT_FAILURE; }
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
        || ExtractJsonDouble("{\"value\":1e999}", "value").has_value()
        || ExtractJsonInt("{\"value\":01}", "value").has_value()
        || ExtractJsonInt("{\"value\":-01}", "value").has_value()
        || ExtractJsonUInt64("{\"value\":00}", "value").has_value()
        || ExtractJsonUInt64("{\"value\":18446744073709551616}", "value").has_value()
        || ExtractJsonInt("{\"value\":2147483648}", "value").has_value()
        || ExtractJsonInt("{\"value\":-2147483649}", "value").has_value()) {
        return EXIT_FAILURE;
    }
    if (ExtractJsonInt("{\"value\":-2147483648}", "value") != std::numeric_limits<std::int32_t>::min()
        || ExtractJsonUInt64("{\"value\":18446744073709551615}", "value")
            != std::numeric_limits<std::uint64_t>::max()
        || ExtractJsonDouble("{\"value\":-2.5e+2}", "value") != -250.0) {
        return EXIT_FAILURE;
    }
    const std::vector<std::string> validArray = ExtractJsonStringArray("{\"items\":[\"a\",\"b\"]}", "items");
    if (validArray != std::vector<std::string>{"a", "b"}
        || !ExtractJsonStringArray("{\"items\":[\"a\" \"b\"]}", "items").empty()
        || !ExtractJsonStringArray("{\"items\":[\"a\",]}", "items").empty()
        || !ExtractJsonStringArray("{\"items\":[\"a\"}", "items").empty()
        || !ExtractJsonStringArray("{\"items\":[\"a\"]junk}", "items").empty()) {
        return EXIT_FAILURE;
    }

    const std::vector<std::string_view> validObjects =
        SplitJsonArrayObjects("{\"items\":[{\"id\":1},{\"id\":2}]}", "items");
    if (validObjects.size() != 2U
        || !SplitJsonArrayObjects("{\"items\":[{\"id\":1} {\"id\":2}]}", "items").empty()
        || !SplitJsonArrayObjects("{\"items\":[{\"id\":1},]}", "items").empty()
        || !SplitJsonArrayObjects("{\"items\":[{\"id\":1}", "items").empty()
        || !SplitJsonArrayObjects("{\"items\":[42]}", "items").empty()
        || !SplitJsonArrayObjects("{\"items\":[{\"id\":1}]junk}", "items").empty()) {
        return EXIT_FAILURE;
    }

    if (ExtractJsonObject("{\"object\":{\"text\":\"} still text\"}}", "object")
            != std::optional<std::string_view>{"{\"text\":\"} still text\"}"}
        || ExtractJsonObject("{\"object\":{\"bad\":\"\\q\"}}", "object").has_value()
        || ExtractJsonObject("{\"object\":{}junk}", "object").has_value()
        || FindJsonKey("{\"broken\":\"unterminated, \"target\":42}", "target").has_value()) {
        return EXIT_FAILURE;
    }

    std::string controls;
    controls.push_back('\0');
    controls.push_back('\x01');
    controls.push_back('\x1F');
    if (EscapeJsonString(controls) != "\\u0000\\u0001\\u001F") {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
