# Unreal C++ reviewer

Inspect UHT signatures, build/module boundaries, UObject lifetimes, game-thread requirements, async result ownership and teardown. Confirm no gameplay module dependency and no authored asset references to editor plugin types. Use public UE5.8 APIs and evaluate compile/save semantics. Report concrete file-level defects and reproduce when possible; a static review is not successful build or runtime evidence.
