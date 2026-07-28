#include <spdlog/sinks/basic_file_sink.h>
#include <filesystem>

#include <ShlObj.h>
#include <KnownFolders.h>

namespace {
    std::filesystem::path ResolveLogDirectory() {
        wchar_t* docs = nullptr;
        std::filesystem::path p;
        if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &docs))) {
            p = docs;
            ::CoTaskMemFree(docs);
        } else {
            const wchar_t* up = _wgetenv(L"USERPROFILE");
            if (up) p = std::filesystem::path(up) / "Documents";
        }
        p /= "My Games";
        p /= "Skyrim Special Edition";
        p /= "SKSE";
        return p;
    }

    void InitializeLogging() {
        try {
            auto dir = ResolveLogDirectory();
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            auto path = dir / "LockDoors.log";
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
            auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
            log->set_level(spdlog::level::info);
            log->flush_on(spdlog::level::info);
            spdlog::set_default_logger(std::move(log));
            spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
        } catch (...) {}
    }

    // Pops "Door is Locked" when the player tries to open a door our mod holds shut via
    // BlockActivation. That is a SEPARATE system from vanilla key-locks (GetLocked), so this
    // never fires on normal locked/key doors — only on doors held shut by BlockActivation.
    class ActivateSink : public RE::BSTEventSink<RE::TESActivateEvent> {
    public:
        static ActivateSink* GetSingleton() {
            static ActivateSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESActivateEvent* a_event,
                                              RE::BSTEventSource<RE::TESActivateEvent>*) override {
            if (!a_event) {
                return RE::BSEventNotifyControl::kContinue;
            }

            auto* obj = a_event->objectActivated.get();
            auto* act = a_event->actionRef.get();
            if (!obj || !act) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Only when the PLAYER is doing the activating.
            if (act != RE::PlayerCharacter::GetSingleton()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Doors only.
            auto* base = obj->GetBaseObject();
            if (!base || base->GetFormType() != RE::FormType::Door) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Only doors held shut via BlockActivation (our lock).
            if (!obj->IsActivationBlocked()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            RE::DebugNotification("Door is Locked");
            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        ActivateSink() = default;
    };

    void MessageCallback(SKSE::MessagingInterface::Message* msg) {
        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
                holder->AddEventSink<RE::TESActivateEvent>(ActivateSink::GetSingleton());
                SKSE::log::info("LockDoors: TESActivateEvent sink registered");
            } else {
                SKSE::log::error("LockDoors: ScriptEventSourceHolder is null");
            }
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    InitializeLogging();
    SKSE::log::info("LockDoors loading...");
    SKSE::Init(skse);

    auto* mi = SKSE::GetMessagingInterface();
    if (!mi || !mi->RegisterListener(MessageCallback)) {
        SKSE::log::error("Failed to register SKSE messaging listener");
        return false;
    }
    SKSE::log::info("LockDoors loaded");
    return true;
}
