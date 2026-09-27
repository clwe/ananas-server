#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

// Runs all juce::UnitTest instances; returns non-zero if any test fails.
int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();

    int numFailures{0};
    for (int i{0}; i < runner.getNumResults(); ++i) {
        numFailures += runner.getResult(i)->failures;
    }

    std::cout << (numFailures == 0 ? "All tests passed." : juce::String{numFailures} + " failure(s).") << std::endl;
    return numFailures == 0 ? 0 : 1;
}
