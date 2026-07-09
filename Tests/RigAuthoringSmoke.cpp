#include "RawIron/Scene/RigAuthoring.h"

#include <cstdlib>
#include <optional>
#include <string>

int main() {
    const ri::scene::RigDefinition humanoid = ri::scene::CreateHumanoidRigDefinition("test_humanoid", "Test Humanoid");
    const ri::scene::RigValidationReport report = ri::scene::ValidateRigDefinition(humanoid);
    if (!report.valid || report.rootBoneCount != 1U || report.humanoidMatchedBoneCount != report.humanoidRequiredBoneCount) {
        return EXIT_FAILURE;
    }

    const std::optional<ri::scene::RigDefinition> parsed =
        ri::scene::ParseRigDefinition(ri::scene::SerializeRigDefinition(humanoid));
    if (!parsed.has_value() || parsed->bones.size() != humanoid.bones.size()
        || parsed->profile != ri::scene::RigProfile::Humanoid) {
        return EXIT_FAILURE;
    }

    ri::scene::RigDefinition invalid = humanoid;
    invalid.bones[1].name = invalid.bones[0].name;
    invalid.bones[2].parentIndex = 3;
    invalid.bones[3].parentIndex = 2;
    const ri::scene::RigValidationReport invalidReport = ri::scene::ValidateRigDefinition(invalid);
    if (invalidReport.valid || invalidReport.errors.size() < 2U) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
