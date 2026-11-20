#include "PluginEditor.h"

namespace
{
    const juce::Colour bg      { 0xff14161b };
    const juce::Colour panel   { 0xff1e2128 };
    const juce::Colour textCol { 0xffd9dde6 };
}

VocalStackEditor::Section& VocalStackEditor::addSection (const juce::String& title, juce::Colour colour, const char* powerId)
{
    auto s = std::make_unique<Section>();
    s->title = title;
    s->colour = colour;
    s->power.setButtonText ("");
    s->power.setColour (juce::ToggleButton::tickColourId, colour);
    s->powerAttach = std::make_unique<ButtonAttach> (proc.apvts, powerId, s->power);
    addAndMakeVisible (s->power);
    sections.push_back (std::move (s));
    return *sections.back();
}

void VocalStackEditor::addKnob (Section& s, const char* id, const char* label)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, s.colour);
    k->slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff343944));
    k->slider.setColour (juce::Slider::thumbColourId, s.colour.brighter (0.4f));
    k->slider.setColour (juce::Slider::textBoxTextColourId, textCol);
    k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, textCol.withAlpha (0.7f));
    k->label.setFont (juce::FontOptions (12.0f));
    k->attach = std::make_unique<SliderAttach> (proc.apvts, id, k->slider);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);
    s.knobs.push_back (std::move (k));
}

void VocalStackEditor::addCombo (Section& s, const char* id)
{
    auto idx = s.numCombos++;
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id));
    if (param != nullptr)
        s.combo[idx].addItemList (param->choices, 1);
    s.comboAttach[idx] = std::make_unique<ComboAttach> (proc.apvts, id, s.combo[idx]);
    addAndMakeVisible (s.combo[idx]);
}

VocalStackEditor::VocalStackEditor (VocalStackProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    auto& pitch = addSection ("PITCH", juce::Colour (0xffff5c7a), "pitch_on");
    addCombo (pitch, "pitch_key");
    addCombo (pitch, "pitch_scale");
    addKnob (pitch, "pitch_speed", "Speed");
    addKnob (pitch, "pitch_amount", "Amount");

    auto& eq = addSection ("EQ", juce::Colour (0xff4fc3f7), "eq_on");
    addKnob (eq, "eq_hpf", "Low Cut");
    addKnob (eq, "eq_low", "Body");
    addKnob (eq, "eq_mid", "Presence");
    addKnob (eq, "eq_air", "Air");

    auto& ds = addSection ("DE-ESS", juce::Colour (0xff80cbc4), "ds_on");
    addKnob (ds, "ds_freq", "Freq");
    addKnob (ds, "ds_thresh", "Thresh");

    auto& cp = addSection ("COMPRESSOR", juce::Colour (0xffffb74d), "cp_on");
    addKnob (cp, "cp_thresh", "Thresh");
    addKnob (cp, "cp_ratio", "Ratio");
    addKnob (cp, "cp_attack", "Attack");
    addKnob (cp, "cp_release", "Release");
    addKnob (cp, "cp_makeup", "Makeup");

    auto& sat = addSection ("SATURATION", juce::Colour (0xffff8a65), "sat_on");
    addKnob (sat, "sat_drive", "Drive");
    addKnob (sat, "sat_mix", "Mix");

    auto& dbl = addSection ("DOUBLER", juce::Colour (0xffba68c8), "dbl_on");
    addKnob (dbl, "dbl_rate", "Rate");
    addKnob (dbl, "dbl_depth", "Depth");
    addKnob (dbl, "dbl_mix", "Mix");

    auto& dly = addSection ("DELAY", juce::Colour (0xff7986cb), "dly_on");
    addKnob (dly, "dly_time", "Time");
    addKnob (dly, "dly_fb", "Feedback");
    addKnob (dly, "dly_tone", "Tone");
    addKnob (dly, "dly_mix", "Mix");

    auto& rev = addSection ("REVERB", juce::Colour (0xff4db6ac), "rev_on");
    addKnob (rev, "rev_size", "Size");
    addKnob (rev, "rev_damp", "Damping");
    addKnob (rev, "rev_mix", "Mix");

    // Output has no bypass: reuse a hidden toggle-less section by giving it the EQ power? No -
    // build it as a plain knob that lives with the Reverb row instead.
    auto out = std::make_unique<Section>();
    out->title = "OUTPUT";
    out->colour = juce::Colour (0xffe0e0e0);
    out->power.setVisible (false);
    sections.push_back (std::move (out));
    addKnob (*sections.back(), "out_gain", "Level");

    setSize (940, 640);
}

void VocalStackEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.setColour (textCol);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("VOCALSTACK", 20, 10, 300, 30, juce::Justification::centredLeft);
    g.setColour (textCol.withAlpha (0.45f));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("vocal multi-effect", 170, 10, 200, 30, juce::Justification::centredLeft);

    for (auto& s : sections)
    {
        auto b = s->bounds.toFloat();
        g.setColour (panel);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (s->colour.withAlpha (0.8f));
        g.fillRoundedRectangle (b.getX(), b.getY(), b.getWidth(), 3.0f, 1.5f);
        g.setColour (s->colour);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (s->title, (int) b.getX() + 40, (int) b.getY() + 8, (int) b.getWidth() - 50, 22,
                    juce::Justification::centredLeft);
    }
}

void VocalStackEditor::resized()
{
    // Three rows of sections; widths are proportional to the number of controls.
    const std::vector<std::vector<int>> rows { { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 } };
    auto area = getLocalBounds().withTrimmedTop (48).reduced (14, 6);
    const int rowH = area.getHeight() / 3;

    for (int r = 0; r < 3; ++r)
    {
        auto row = area.removeFromTop (rowH);
        int totalUnits = 0;
        for (int idx : rows[(size_t) r])
            totalUnits += juce::jmax (2, (int) sections[(size_t) idx]->knobs.size());

        for (int idx : rows[(size_t) r])
        {
            auto& s = *sections[(size_t) idx];
            const int units = juce::jmax (2, (int) s.knobs.size());
            auto cell = row.removeFromLeft (row.getWidth() * units / juce::jmax (1, totalUnits)).reduced (5);
            totalUnits -= units;
            s.bounds = cell;

            s.power.setBounds (cell.getX() + 10, cell.getY() + 8, 24, 22);

            auto inner = cell.reduced (8, 0).withTrimmedTop (34);

            if (s.numCombos > 0)
            {
                auto comboRow = inner.removeFromTop (24);
                const int cw = comboRow.getWidth() / s.numCombos;
                for (int c = 0; c < s.numCombos; ++c)
                    s.combo[c].setBounds (comboRow.removeFromLeft (cw).reduced (3, 0));
                inner.removeFromTop (4);
            }

            if (! s.knobs.empty())
            {
                const int kw = inner.getWidth() / (int) s.knobs.size();
                for (auto& k : s.knobs)
                {
                    auto kc = inner.removeFromLeft (kw);
                    k->label.setBounds (kc.removeFromTop (16));
                    k->slider.setBounds (kc);
                }
            }
        }
    }
    repaint();
}
