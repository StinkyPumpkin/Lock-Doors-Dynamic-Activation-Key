Scriptname LockDoors_QuestScript Extends ReferenceAlias

Perk Property LockDoors_LockPerk Auto

Event OnInit()
    AddLockPerk()
    RestoreLockedDoors()
EndEvent

Event OnPlayerLoadGame()
    AddLockPerk()
    RestoreLockedDoors()
EndEvent

Function AddLockPerk()
    Actor player = GetActorReference()
    If player && !player.HasPerk(LockDoors_LockPerk)
        player.AddPerk(LockDoors_LockPerk)
    EndIf
EndFunction

; On load, re-apply SetActivationBlocked to all doors we locked
Function RestoreLockedDoors()
    Actor player = GetActorReference()
    If !player
        Return
    EndIf

    ; --Claude: re-tag locked doors with the keyword too, so the perk's Lock/Unlock/Locked
    ; label survives a save/reload (runtime keywords + BlockActivation both re-applied here).
    Keyword lockedKW = Game.GetFormFromFile(0x810, "LockDoors.esp") as Keyword

    Int count = StorageUtil.FormListCount(player, "LockDoors_Locked")
    Int i = 0
    While i < count
        ObjectReference doorRef = StorageUtil.FormListGet(player, "LockDoors_Locked", i) as ObjectReference
        If doorRef
            doorRef.BlockActivation(true)
            If lockedKW
                PO3_SKSEFunctions.AddKeywordToRef(doorRef, lockedKW)
            EndIf
        EndIf
        i += 1
    EndWhile
EndFunction
