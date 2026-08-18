#pragma once

#include "ui/ListSelectorPopup.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

// OK / Cancel over a message. Cancel, Escape and outside click all dismiss with no effect;
// Cancel exists so the dismiss path is visible rather than folklore.
inline std::unique_ptr<ListSelectorPopup> MakeConfirmPopup(std::string message,
                                                           WindowLayout_t layout,
                                                           std::function<void()> onOk,
                                                           const ListSelectorPopupStyle_t& rStyle)
{
    std::vector<PopupChoice_t> choices;
    choices.push_back({"OK", std::move(onOk)});
    choices.push_back({"Cancel", [] {}});
    return std::make_unique<ListSelectorPopup>(std::move(message), "", std::move(choices), layout,
                                               rStyle);
}

} // namespace ac
