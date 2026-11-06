#include "mod/ai_play/evaluation.hpp"

#include <cassert>
#include <sstream>

int main() {
    using namespace aiplay;
    using namespace aiplay::brain;
    using namespace aiplay::evaluation;

    Model model;
    // Two stable Wave contexts with balanced labels. Identical held-out contexts
    // make this a deterministic smoke test for split isolation and metric accounting.
    for (int i = 0; i < 100; ++i) {
        TrainingExample e;
        e.mechanic = Mechanic::Wave;
        e.verticalVelocity = i < 50 ? -8.0 : 8.0;
        e.vehicleSize = 1.0;
        e.hazardGap = i < 50 ? 18.0 : 72.0;
        e.hazardVerticalOffset = i < 50 ? -12.0 : 12.0;
        e.nearbyHazardCount = 2;
        e.buttonHeldBefore = (i % 2) != 0;
        e.targetHeld = i >= 50;
        model.examples.push_back(e);
    }

    const auto report = evaluate(model, 5);
    assert(report.holdoutExamples == 20);
    assert(report.trainingExamples == 80);
    assert(report.overall.samples == 20);
    assert(report.mechanics[static_cast<std::size_t>(Mechanic::Wave)].samples == 20);
    assert(report.overall.evaluated + report.overall.abstained == 20);
    assert(report.overall.correct + report.overall.incorrect ==
           report.overall.evaluated);
    assert(report.overall.coverage() >= 0.0 && report.overall.coverage() <= 1.0);
    assert(report.overall.accuracyWhenActing() >= 0.0 &&
           report.overall.accuracyWhenActing() <= 1.0);

    std::ostringstream csv;
    writeCsv(csv, report);
    const auto output = csv.str();
    assert(output.find("mechanic,samples,evaluated") == 0);
    assert(output.find("\nwave,") != std::string::npos);
    assert(output.find("\nship,") != std::string::npos);

    // An empty model must be reported as full abstention, never as a successful policy.
    Model empty;
    const auto emptyReport = evaluate(empty);
    assert(emptyReport.overall.samples == 0);
    assert(emptyReport.overall.coverage() == 0.0);
    assert(emptyReport.overall.accuracyWhenActing() == 0.0);
    return 0;
}
