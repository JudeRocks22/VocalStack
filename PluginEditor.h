#pragma once
#include "PluginProcessor.h"

class VocalStackEditor : public juce::AudioProcessorEditor
{
public:
    explicit VocalStackEditor (VocalStackProcessor&);
    ~VocalStackEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttach> attach;
    };

    struct Section
    {
        juce::String title;
        juce::Colour colour;
        juce::ToggleButton power;
        std::unique_ptr<ButtonAttach> powerAttach;
        std::vector<std::unique_ptr<Knob>> knobs;
        juce::ComboBox combo[2];
        std::unique_ptr<ComboAttach> comboAttach[2];
        int numCombos = 0;
        juce::Rectangle<int> bounds;
    };

    Section& addSection (const juce::String& title, juce::Colour colour, const char* powerId);
    void addKnob (Section&, const char* id, const char* label);
    void addCombo (Section&, const char* id);

    VocalStackProcessor& proc;
    std::vector<std::unique_ptr<Section>> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalStackEditor)
};
