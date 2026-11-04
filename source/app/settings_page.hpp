#pragma once

#include <borealis.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace mm {
class Library;
}

namespace app {

class SettingsPage : public brls::Box {
public:
    SettingsPage();
    ~SettingsPage() override;

private:
    void revealNsfwSetting();
    // Run a library job behind a "please wait" screen; `doneText` is notified on success.
    void runJob(brls::View* cell, const std::string& status, std::function<void(mm::Library&, std::function<void(uint64_t, uint64_t)>)> job,
                const std::string& doneText, std::function<void()> onSuccess = {});

    brls::ScrollingFrame* scroll_ = nullptr;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::vector<brls::View*> nsfwViews_;  // hidden until L + R + minus is held
    brls::RepeatingTimer holdTimer_;
    int heldMs_ = 0;
};

}  // namespace app
