#include "API/ARK/Ark.h"

struct FFrame
{
    UFunction*& NodeField() { return *GetNativePointerField<UFunction**>(this, "FFrame.Node"); }
};

UFunction* CryoUbergraphFunc;

// ArkServerApi hook used to add dino stat points to the cryopod widget
DECLARE_HOOK(UPrimalItem_execSetCustomItemData, void, UPrimalItem*, FFrame*, void* const);
void Hook_UPrimalItem_execSetCustomItemData(UPrimalItem* _this, FFrame* Stack, void* const Result)
{
    AShooterWeapon* cryo;
    UProperty* dinoProp;
    UProperty* dataProp;
    APrimalDinoCharacter** dino;
    FCustomItemData* data;
    char* naturalStats;
    char* tamedStats;
    std::string newName;

    // Check if this function is being called while capturing a dino
    if (Stack->NodeField() == CryoUbergraphFunc) {
        cryo = _this->AssociatedWeaponField().Get();
        if (cryo == nullptr) {
            Log::GetLog()->error("NULL: AssociatedWeapon");
        } else {
            // Get the UProperties that point to the dino and the FCustomItemData being set
            dinoProp = cryo->FindProperty(FName("CapturingDino", EFindName::FNAME_Find));
            dataProp = cryo->FindProperty(FName("K2Node_MakeStruct_CustomItemData", EFindName::FNAME_Find));
            if ((dinoProp == nullptr) || (dinoProp->ElementSizeField() != sizeof(APrimalDinoCharacter*))) {
                Log::GetLog()->error("UProperty not found: CapturingDino");
            } else if ((dataProp == nullptr) || (dataProp->ElementSizeField() != sizeof(FCustomItemData))) {
                Log::GetLog()->error("UProperty not found: K2Node_MakeStruct_CustomItemData");
            } else {
                dino = (APrimalDinoCharacter**)((uint8*)cryo + dinoProp->Offset_InternalField());
                data = (FCustomItemData*)((uint8*)cryo + dataProp->Offset_InternalField());
                naturalStats = (*dino)->GetCharacterStatusComponent()->NumberOfLevelUpPointsAppliedField()();
                tamedStats = (*dino)->GetCharacterStatusComponent()->NumberOfLevelUpPointsAppliedTamedField()();

                // Append stat points to the dino's name string displayed by the cryopod
                newName = data->CustomDataStrings[1].ToString();
                newName += "\n";
                newName += "\nHealth:      " + std::to_string(naturalStats[EPrimalCharacterStatusValue::Health]);
                newName += (tamedStats[EPrimalCharacterStatusValue::Health] == 0) ? "" : " +" + std::to_string(tamedStats[EPrimalCharacterStatusValue::Health]);
                newName += "\nStamina:   " + std::to_string(naturalStats[EPrimalCharacterStatusValue::Stamina]);
                newName += (tamedStats[EPrimalCharacterStatusValue::Stamina] == 0) ? "" : " +" + std::to_string(tamedStats[EPrimalCharacterStatusValue::Stamina]);
                newName += "\nOxygen:     " + std::to_string(naturalStats[EPrimalCharacterStatusValue::Oxygen]);
                newName += (tamedStats[EPrimalCharacterStatusValue::Oxygen] == 0) ? "" : " +" + std::to_string(tamedStats[EPrimalCharacterStatusValue::Oxygen]);
                newName += "\nFood:          " + std::to_string(naturalStats[EPrimalCharacterStatusValue::Food]);
                newName += (tamedStats[EPrimalCharacterStatusValue::Food] == 0) ? "" : " +" + std::to_string(tamedStats[EPrimalCharacterStatusValue::Food]);
                newName += "\nWeight:       " + std::to_string(naturalStats[EPrimalCharacterStatusValue::Weight]);
                newName += (tamedStats[EPrimalCharacterStatusValue::Weight] == 0) ? "" : " +" + std::to_string(tamedStats[EPrimalCharacterStatusValue::Weight]);
                newName += "\nDamage:    " + std::to_string(naturalStats[EPrimalCharacterStatusValue::MeleeDamageMultiplier]);
                newName += (tamedStats[EPrimalCharacterStatusValue::MeleeDamageMultiplier] == 0) ? "" : " +" + std::to_string(tamedStats[EPrimalCharacterStatusValue::MeleeDamageMultiplier]);
                newName += "\nSpeed:         " + std::to_string(naturalStats[EPrimalCharacterStatusValue::SpeedMultiplier]);
                newName += (tamedStats[EPrimalCharacterStatusValue::SpeedMultiplier] == 0) ? "" : " +" + std::to_string(tamedStats[EPrimalCharacterStatusValue::SpeedMultiplier]);

                data->CustomDataStrings[1] = FString(newName);
            }
        }
    }

    UPrimalItem_execSetCustomItemData_original(_this, Stack, Result);
}

// Called when the server is ready
void Plugin_ServerReadyInit()
{
    CryoUbergraphFunc = (UFunction*)Globals::FindClass("Function /Game/Extinction/CoreBlueprints/Weapons/WeapEmptyCryopod.WeapEmptyCryopod_C:ExecuteUbergraph_WeapEmptyCryopod");
    if (CryoUbergraphFunc == nullptr)
        Log::GetLog()->error("UFunction not found: WeapEmptyCryopod_C::ExecuteUbergraph_WeapEmptyCryopod");
}

// ArkServerApi hook that triggers once when the server is ready
DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);
void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* _this)
{
    Log::GetLog()->info("Hook_AShooterGameMode_BeginPlay()");
    AShooterGameMode_BeginPlay_original(_this);

    // Call Plugin_ServerReadyInit() for post-"server ready" initialization
    Plugin_ServerReadyInit();
}

// Called by ArkServerApi when the plugin is loaded
extern "C" __declspec(dllexport) void Plugin_Init()
{
    Log::Get().Init(PROJECT_NAME);

    ArkApi::GetHooks().SetHook("AShooterGameMode.BeginPlay",
        Hook_AShooterGameMode_BeginPlay,
        &AShooterGameMode_BeginPlay_original);
    ArkApi::GetHooks().SetHook("UPrimalItem.execSetCustomItemData",
        Hook_UPrimalItem_execSetCustomItemData,
        &UPrimalItem_execSetCustomItemData_original);

    // If the server is ready, call Plugin_ServerReadyInit() for post-"server ready" initialization
    if (ArkApi::GetApiUtils().GetStatus() == ArkApi::ServerStatus::Ready)
        Plugin_ServerReadyInit();
}

// Called by ArkServerApi when the plugin is unloaded
extern "C" __declspec(dllexport) void Plugin_Unload()
{
    ArkApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay", Hook_AShooterGameMode_BeginPlay);
    ArkApi::GetHooks().DisableHook("UPrimalItem.execSetCustomItemData", Hook_UPrimalItem_execSetCustomItemData);
}