#include "API/ARK/Ark.h"

struct FFrame
{
    UFunction*& NodeField() { return *GetNativePointerField<UFunction**>(this, "FFrame.Node"); }
    UObject*& ObjectField() { return *GetNativePointerField<UObject**>(this, "FFrame.Object"); }
    uint8*& CodeField() { return *GetNativePointerField<uint8**>(this, "FFrame.Code"); }
    FFrame*& PreviousFrameField() { return *GetNativePointerField<FFrame**>(this, "FFrame.PreviousFrame"); }
};

UFunction* CryoUbergraphFunc;
UFunction* CryoIngestFunc;

// Read a uint64 from a BP script
uint64 ReadQWORD(unsigned char* Script)
{
    int scriptIndex = 0;
    uint64 Value = Script[scriptIndex++];
    Value = Value | ((uint64)Script[scriptIndex++] << 8);
    Value = Value | ((uint64)Script[scriptIndex++] << 16);
    Value = Value | ((uint64)Script[scriptIndex++] << 24);
    Value = Value | ((uint64)Script[scriptIndex++] << 32);
    Value = Value | ((uint64)Script[scriptIndex++] << 40);
    Value = Value | ((uint64)Script[scriptIndex++] << 48);
    Value = Value | ((uint64)Script[scriptIndex] << 56);

    return Value;
}

// ArkServerApi hook used to add dino stat points to the cryopod widget when a dino is captured
DECLARE_HOOK(UPrimalItem_execSetCustomItemData, void, UPrimalItem*, FFrame*, void* const);
void Hook_UPrimalItem_execSetCustomItemData(UPrimalItem* _this, FFrame* Stack, void* const Result)
{
    AShooterWeapon* cryo;
    UProperty* dinoProp;
    UProperty* dataProp = nullptr;
    APrimalDinoCharacter** dino;
    FCustomItemData* data;
    char* naturalStats;
    char* tamedStats;
    std::string newName;

    // Check if this function is being called while capturing a dino
    if ((Stack->NodeField() != CryoUbergraphFunc) &&
        (Stack->PreviousFrameField() == nullptr) &&
        (Stack->PreviousFrameField()->NodeField() != CryoIngestFunc))
    {
        UPrimalItem_execSetCustomItemData_original(_this, Stack, Result);
        return;
    }

    // Get the cryopod weapon
    cryo = (AShooterWeapon*)Stack->PreviousFrameField()->ObjectField();

    // Get the UProperty for the APrimalDinoCharacter* instance variable that points to the dino being captured
    dinoProp = cryo->FindProperty(FName("CapturingDino", EFindName::FNAME_Find));

    // Get the UProperty for the FCustomItemData instance variable that describes the captured dino
    if (*Stack->CodeField() == (uint8)EExprToken::EX_InstanceVariable)
        dataProp = (UProperty*)ReadQWORD(Stack->CodeField() + 1);

    if ((dinoProp == nullptr) || (dinoProp->ElementSizeField() != sizeof(APrimalDinoCharacter*)))
    {
        Log::GetLog()->error("UProperty not found: CapturingDino");
    }
    else if ((dataProp == nullptr) || (dataProp->ElementSizeField() != sizeof(FCustomItemData)))
    {
        Log::GetLog()->error("UProperty not found: InData");
    }
    else
    {
        dino = (APrimalDinoCharacter**)((uint8*)cryo + dinoProp->Offset_InternalField());
        data = (FCustomItemData*)((uint8*)cryo + dataProp->Offset_InternalField());
        naturalStats = (*dino)->MyCharacterStatusComponentField()->NumberOfLevelUpPointsAppliedField()();
        tamedStats = (*dino)->MyCharacterStatusComponentField()->NumberOfLevelUpPointsAppliedTamedField()();

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

    UPrimalItem_execSetCustomItemData_original(_this, Stack, Result);
}

// Called when the server is ready
void Plugin_ServerReadyInit()
{
    CryoIngestFunc = (UFunction*)Globals::FindClass("Function /Game/Extinction/CoreBlueprints/Weapons/WeapEmptyCryopod.WeapEmptyCryopod_C:Ingest");
    if (CryoIngestFunc == nullptr)
        Log::GetLog()->error("UFunction not found: WeapEmptyCryopod_C::Ingest");

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