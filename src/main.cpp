#include <SkyPrompt/API.hpp>
#include <unordered_set>

static SkyPromptAPI::ClientID g_clientID = 0;
static RE::TESObjectREFR*     g_crosshairRef = nullptr;
static std::unordered_set<RE::FormID> g_lockedDoors;
static bool g_promptShowing = false;

static constexpr SkyPromptAPI::ActionID kActionLock   = 1;
static constexpr SkyPromptAPI::ActionID kActionUnlock = 2;

// Persistent string storage — SkyPrompt::Prompt takes string_view,
// so the backing string must outlive the prompt.
static constexpr std::string_view kTextLock   = "Lock";
static constexpr std::string_view kTextUnlock = "Unlock";

static const std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID> kKeys[] = {
    { RE::INPUT_DEVICE::kKeyboard, 19 },   // R
    { RE::INPUT_DEVICE::kGamepad, 0x4000 }, // X button
};

static bool IsDoor(RE::TESObjectREFR* ref) {
    if (!ref) return false;
    auto* base = ref->GetBaseObject();
    return base && base->Is(RE::FormType::Door);
}

static bool IsClosed(RE::TESObjectREFR* ref) {
    if (!ref) return false;
    auto state = RE::BGSOpenCloseForm::GetOpenState(ref);
    return state == RE::BGSOpenCloseForm::OPEN_STATE::kClosed ||
           state == RE::BGSOpenCloseForm::OPEN_STATE::kClosing;
}

static void PlayLockSound(RE::TESObjectREFR* ref) {
    auto* audio = RE::BSAudioManager::GetSingleton();
    if (!audio) return;
    RE::BSSoundHandle handle;
    audio->BuildSoundDataFromEditorID(handle, "UILockpickingCylinderSqueeze", 0x10);
    if (handle.IsValid()) {
        if (auto* node = ref->Get3D())
            handle.SetObjectToFollow(node);
        handle.Play();
    }
}

// ── SkyPrompt sink ────────────────────────────────────────────────
// SkyPrompt calls GetPrompts() once per SendPrompt() call, takes the
// returned prompts, and displays them until RemovePrompt() is called.
// It does NOT poll GetPrompts continuously.
//
// Our CrosshairHandler drives the lifecycle:
//   crosshair → valid door  → update m_prompt → SendPrompt()
//   crosshair → leaves door → RemovePrompt()

class DoorLockSink final : public SkyPromptAPI::PromptSink {
    mutable SkyPromptAPI::Prompt m_prompt;

public:
    // Called by SkyPrompt once per SendPrompt — return the current prompt.
    std::span<const SkyPromptAPI::Prompt> GetPrompts() const override {
        return { &m_prompt, 1 };
    }

    // Called when the user interacts with the prompt.
    void ProcessEvent(SkyPromptAPI::PromptEvent event) const override {
        if (event.type != SkyPromptAPI::kAccepted) return;

        // Use the current crosshair ref (refid is 0 so it doesn't anchor to the door)
        auto* ref = g_crosshairRef;
        if (!ref) return;

        if (event.prompt.actionID == kActionLock) {
            g_lockedDoors.insert(ref->GetFormID());
            SKSE::GetTaskInterface()->AddTask([ref]() {
                ref->SetActivationBlocked(true);
                PlayLockSound(ref);
                RE::DebugNotification("Locked");
            });
            SKSE::log::info("LockDoors: Locked {} (0x{:08X})", ref->GetName(), ref->GetFormID());

            // After locking, refresh the prompt to show "Unlock"
            RefreshPrompt();
        }
        else if (event.prompt.actionID == kActionUnlock) {
            g_lockedDoors.erase(ref->GetFormID());
            SKSE::GetTaskInterface()->AddTask([ref]() {
                ref->SetActivationBlocked(false);
                PlayLockSound(ref);
                RE::DebugNotification("Unlocked");
            });
            SKSE::log::info("LockDoors: Unlocked {} (0x{:08X})", ref->GetName(), ref->GetFormID());

            // After unlocking, refresh the prompt to show "Lock"
            RefreshPrompt();
        }
    }

    // Update the prompt data for the current crosshair ref.
    // Returns true if a valid prompt was set, false if crosshair isn't on a lockable door.
    bool UpdatePromptForRef(RE::TESObjectREFR* ref) {
        if (!ref || !IsDoor(ref)) return false;

        auto formID = ref->GetFormID();
        bool isLocked = g_lockedDoors.contains(formID);

        if (isLocked) {
            m_prompt = SkyPromptAPI::Prompt(
                kTextUnlock, 0, kActionUnlock,
                SkyPromptAPI::kSinglePress, 0,
                std::span(kKeys));
            return true;
        }

        if (IsClosed(ref)) {
            m_prompt = SkyPromptAPI::Prompt(
                kTextLock, 0, kActionLock,
                SkyPromptAPI::kSinglePress, 0,
                std::span(kKeys));
            return true;
        }

        return false; // door is open, can't lock
    }

    // Show or hide the prompt based on current crosshair state.
    void RefreshPrompt() const {
        if (!g_clientID) return;

        auto* ref = g_crosshairRef;
        bool shouldShow = const_cast<DoorLockSink*>(this)->UpdatePromptForRef(ref);

        if (shouldShow && !g_promptShowing) {
            // Start showing
            if (SkyPromptAPI::SendPrompt(this, g_clientID)) {
                g_promptShowing = true;
                SKSE::log::trace("LockDoors: Prompt shown for 0x{:08X}", ref->GetFormID());
            }
        }
        else if (shouldShow && g_promptShowing) {
            // Already showing but need to update (e.g., lock→unlock transition)
            SkyPromptAPI::RemovePrompt(this, g_clientID);
            if (SkyPromptAPI::SendPrompt(this, g_clientID)) {
                SKSE::log::trace("LockDoors: Prompt refreshed for 0x{:08X}", ref->GetFormID());
            } else {
                g_promptShowing = false;
            }
        }
        else if (!shouldShow && g_promptShowing) {
            // Stop showing
            SkyPromptAPI::RemovePrompt(this, g_clientID);
            g_promptShowing = false;
            SKSE::log::trace("LockDoors: Prompt hidden");
        }
    }
};

static DoorLockSink g_sink;

// ── Crosshair tracking ────────────────────────────────────────────
// Each crosshair change drives SendPrompt/RemovePrompt.

class CrosshairHandler final : public RE::BSTEventSink<SKSE::CrosshairRefEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(
        const SKSE::CrosshairRefEvent* event,
        [[maybe_unused]] RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override
    {
        g_crosshairRef = event ? event->crosshairRef.get() : nullptr;

        // Drive prompt visibility from crosshair changes
        g_sink.RefreshPrompt();

        return RE::BSEventNotifyControl::kContinue;
    }

    static CrosshairHandler* GetSingleton() {
        static CrosshairHandler instance;
        return &instance;
    }
};

// ── Cosave ──────────────────────────────────────────────────────────
static constexpr std::uint32_t kCosaveType = 'LKDR';
static constexpr std::uint32_t kCosaveVersion = 1;

static void OnSave(SKSE::SerializationInterface* serde) {
    if (!serde->OpenRecord(kCosaveType, kCosaveVersion)) return;

    auto count = static_cast<std::uint32_t>(g_lockedDoors.size());
    serde->WriteRecordData(&count, sizeof(count));
    for (auto formID : g_lockedDoors)
        serde->WriteRecordData(&formID, sizeof(formID));

    SKSE::log::info("LockDoors: Saved {} locked doors", count);
}

static void OnLoad(SKSE::SerializationInterface* serde) {
    g_lockedDoors.clear();

    std::uint32_t type, version, length;
    while (serde->GetNextRecordInfo(type, version, length)) {
        if (type != kCosaveType) continue;

        std::uint32_t count = 0;
        serde->ReadRecordData(&count, sizeof(count));

        for (std::uint32_t i = 0; i < count; ++i) {
            RE::FormID oldID = 0;
            serde->ReadRecordData(&oldID, sizeof(oldID));

            RE::FormID newID = 0;
            if (serde->ResolveFormID(oldID, newID)) {
                g_lockedDoors.insert(newID);
                // Re-block activation on the loaded door
                SKSE::GetTaskInterface()->AddTask([newID]() {
                    auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(newID);
                    if (ref) ref->SetActivationBlocked(true);
                });
            }
        }
        SKSE::log::info("LockDoors: Loaded {} locked doors", g_lockedDoors.size());
    }
}

static void OnRevert(SKSE::SerializationInterface*) {
    // Remove any active prompt before clearing state
    if (g_promptShowing && g_clientID) {
        SkyPromptAPI::RemovePrompt(&g_sink, g_clientID);
        g_promptShowing = false;
    }
    g_lockedDoors.clear();
    g_crosshairRef = nullptr;
}

// ── SKSE messaging ─────────────────────────────────────────────────
static void OnMessage(SKSE::MessagingInterface::Message* msg) {
    if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
        SKSE::GetCrosshairRefEventSource()->AddEventSink(CrosshairHandler::GetSingleton());

        g_clientID = SkyPromptAPI::RequestClientID();
        SKSE::log::info("LockDoors: ClientID={}", g_clientID);

        if (!g_clientID) {
            SKSE::log::warn("LockDoors: SkyPrompt not available — prompts disabled");
        } else {
            // Request our theme for proper positioning (falls back to default if not found)
            (void)SkyPromptAPI::RequestTheme(g_clientID, "LockDoors");
        }

        // Do NOT call SendPrompt here. The CrosshairHandler will call
        // SendPrompt/RemovePrompt as the player looks at doors.
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    {
        std::filesystem::path logPath =
            "F:\\Documents\\My Games\\Skyrim Special Edition\\SKSE\\LockDoors.log";
        std::filesystem::create_directories(logPath.parent_path());
        try {
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
            auto log  = std::make_shared<spdlog::logger>("LockDoors", std::move(sink));
            log->set_level(spdlog::level::info);
            log->flush_on(spdlog::level::info);
            spdlog::set_default_logger(std::move(log));
        } catch (...) {}
    }

    SKSE::log::info("LockDoors v1.2.0 loaded");
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    auto* serde = SKSE::GetSerializationInterface();
    serde->SetUniqueID(kCosaveType);
    serde->SetSaveCallback(OnSave);
    serde->SetLoadCallback(OnLoad);
    serde->SetRevertCallback(OnRevert);

    return true;
}
