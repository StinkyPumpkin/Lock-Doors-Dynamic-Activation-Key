;BEGIN FRAGMENT CODE - Do not edit anything between this and the end comment
;NEXT FRAGMENT INDEX 1
Scriptname LockDoors_LockPerkScript Extends Perk Hidden

;BEGIN FRAGMENT Fragment_0
Function Fragment_0(ObjectReference akTargetRef, Actor akActor)
    ;BEGIN CODE
    If !akTargetRef
        Return
    EndIf

    ; --Claude: keyword that tags doors WE locked, so the perk can relabel the activate
    ; prompt (Lock / Unlock / Locked). Resolved by FormID so a perk fragment needs no
    ; property fill; guarded, so this no-ops safely if the keyword record isn't present.
    Keyword lockedKW = Game.GetFormFromFile(0x810, "LockDoors.esp") as Keyword

    ; Check if this door is already locked by us
    Int lockIndex = StorageUtil.FormListFind(akActor, "LockDoors_Locked", akTargetRef)

    If lockIndex >= 0
        ; Currently locked → unlock it
        StorageUtil.FormListRemoveAt(akActor, "LockDoors_Locked", lockIndex)
        akTargetRef.BlockActivation(false)
        If lockedKW
            PO3_SKSEFunctions.RemoveKeywordFromRef(akTargetRef, lockedKW)
        EndIf
        Debug.Notification("Unlocked")
    Else
        ; Not locked → check if door is closed first
        Int openState = akTargetRef.GetOpenState()
        ; 1 = Open, 2 = Opening, 3 = Closed, 4 = Closing
        If openState == 3 || openState == 4
            StorageUtil.FormListAdd(akActor, "LockDoors_Locked", akTargetRef)
            akTargetRef.BlockActivation(true)
            If lockedKW
                PO3_SKSEFunctions.AddKeywordToRef(akTargetRef, lockedKW)
            EndIf
            Debug.Notification("Locked")
        Else
            Debug.Notification("Close the door first")
        EndIf
    EndIf
    ;END CODE
EndFunction
;END FRAGMENT

;END FRAGMENT CODE - Do not edit anything between this and the begin comment
