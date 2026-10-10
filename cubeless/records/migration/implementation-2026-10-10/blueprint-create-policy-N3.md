# Blueprint factory save policy N3

The installed UE5.8.3 BlueprintFactory calls FKismetEditorUtilities::CreateBlueprint. Both public overloads in UnrealEd/Public/Kismet2/KismetEditorUtilities.h:110,124 omit compile options. Kismet2.cpp:526–533 performs an initial synchronous compile without SkipSave. The N2 correction to the separate explicit compiler therefore does not cover creation.

The Python UE58 adapter now reads the current BlueprintEditorSettings CDO through the same public ToolsetLibrary property API used by official ObjectTools. The operation checks the policy before parent resolution and mutation intent. The adapter checks again immediately before creating its factory. Non-Never or unavailable policy fails before factory execution; no engine setting is changed. The operation and existing batch action share this guard. A complete new object-path limit below 1024 UTF-16 units also rejects oversized names before FName-sensitive engine calls.

This is an explicit remaining limitation: creation while Always/SuccessOnly is active is unsupported. It is not complete legacy creation parity. Changing global defaults temporarily could affect nested engine callbacks, so this implementation does not do that. No engine sources or gameplay C++ are modified. Third-party callback side effects between observations are not proven absent.

Successful results separately record the factory's implicit compile and its observed Blueprint status, without inventing captured compiler diagnostics. The existing compile result describes explicit action; the additional implicit_factory_compile field makes hidden factory work visible. Explicit compile/save tools still use the N2 SkipSave implementation and retain their three legacy public identities.

Offline plugin tests: 49 passed, including rejection before standalone/batch factory calls, no change intent on policy rejection, and implicit compile metadata. Actual N3 official calls, negative policy probes, cold reload and packaging remain pending at this source checkpoint. N2's six-policy compiler test used a fixture created under Never; it did not validate factory behavior under other policies.
