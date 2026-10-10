"""UE 5.8 public editor transaction scope, never an atomic rollback promise."""

import unreal


def transaction():
    # PyEditor.cpp: BeginTransaction on enter, EndTransaction on exit. An
    # exception does not undo work; persisted saves are outside undo recovery.
    return unreal.ScopedEditorTransaction("Cubeless bounded batch")
