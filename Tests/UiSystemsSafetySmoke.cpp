#include "RawIron/Ui/UiFlowSession.h"
#include "RawIron/Ui/UiJsonIO.h"
#include "RawIron/Ui/UiLayout.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition "\n"; \
            return EXIT_FAILURE; \
        } \
    } while (false)

int main() {
    using namespace ri::ui;

    UiManifest parsed;
    std::string error;
    CHECK(!TryParseUiManifestFromJson(
        R"({"schemaVersion":2,"screens":[{"id":"main"}]})", parsed, &error));
    CHECK(error.find("schemaVersion") != std::string::npos && parsed.screens.empty());
    CHECK(!TryParseUiManifestFromJson(
        R"({"startScreen":"missing","screens":[{"id":"main"}]})", parsed, &error));
    CHECK(error.find("startScreen") != std::string::npos);
    CHECK(!TryParseUiManifestFromJson(
        R"({"screens":[{"id":"main"},{"id":"main"}]})", parsed, &error));
    CHECK(error.find("duplicate UI screen") != std::string::npos);
    CHECK(!TryParseUiManifestFromJson(
        R"({"variables":[{"id":"route","value":"a"},{"id":"route","value":"b"}],"screens":[{"id":"main"}]})",
        parsed,
        &error));
    CHECK(error.find("duplicate UI variable") != std::string::npos);
    CHECK(TryParseUiManifestFromJson(
        R"({"startScreen":"main","variables":[{"id":"route","value":"a"}],"screens":[{"id":"main","background":{"tint":[2,-1,.5,1]},"advance":{"delaySeconds":999999,"action":{"type":"navigate","target":"next"}},"blocks":[{"type":"spacer","height":-5},{"type":"image","heightHint":9999}]},{"id":"next"}]})",
        parsed,
        &error));
    CHECK(error.empty());
    CHECK(parsed.screens.front().backgroundRgba[0] == 1.0f);
    CHECK(parsed.screens.front().backgroundRgba[1] == 0.0f);
    CHECK(parsed.screens.front().advanceAfterSeconds == 86400.0f);
    CHECK(parsed.screens.front().blocks[0].spacerHeight == 0.0f);
    CHECK(parsed.screens.front().blocks[1].imageHeightHint == 2000.0f);

    UiFlowSession session;
    session.Reset(parsed);
    CHECK(session.CurrentScreen() != nullptr && session.CurrentScreen()->id == "main");
    CHECK(session.GetVariableValueView("route") == "a");
    const std::uint64_t resetRevision = session.NavigationRevision();
    for (int index = 0; index < 512; ++index) {
        CHECK(session.NavigateTo("next"));
    }
    CHECK(session.Stack().size() == 128U);
    CHECK(session.Stack().front() == "main" && session.Stack().back() == "next");
    CHECK(session.NavigationRevision() == resetRevision + 512U);
    CHECK(!session.NavigateTo("missing"));

    for (int index = 0; index < 1000; ++index) {
        session.MaybeAppendHistory(
            "line-" + std::to_string(index),
            UiHistoryLine{.text = "line"});
    }
    CHECK(session.History().size() == 160U);

    const UiImageUvRect wide = ComputeCoverImageUv(1920.0f, 1080.0f, 800.0f, 800.0f);
    CHECK(wide.u0 > 0.0f && wide.u1 < 1.0f && wide.v0 == 0.0f && wide.v1 == 1.0f);
    const UiImageUvRect tall = ComputeCoverImageUv(800.0f, 1200.0f, 1600.0f, 900.0f);
    CHECK(tall.v0 > 0.0f && tall.v1 < 1.0f && tall.u0 == 0.0f && tall.u1 == 1.0f);
    const UiStageSize compact = ComputeResponsiveUiStageSize(320.0f, 240.0f);
    CHECK(compact.width <= 296.0f && compact.height <= 204.0f);
    const UiStageSize large = ComputeResponsiveUiStageSize(1920.0f, 1080.0f);
    CHECK(large.width == 900.0f && large.height <= 760.0f);

    return EXIT_SUCCESS;
}
