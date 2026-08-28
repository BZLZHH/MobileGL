// MobileGL - MobileGL/MG_State/GLState/ProgramState/ProgramState.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "ProgramState.h"
#include "MG_State/GLState/SharedObjectTables.h"

namespace MobileGL::MG_State::GLState {
    Uint ProgramState::CreateProgram() {
        auto& programObjects = m_sharedObjectTable ? m_sharedObjectTable->GetProgramObjects() : m_programObjects;
        auto& nameGenerator =
            m_sharedObjectTable ? m_sharedObjectTable->GetNameGenerator() : m_programShaderNameGenerator;
        Uint programId = 0;
        nameGenerator.Generate(1, &programId);
        EnsureIndexAvail(programId, programObjects);
        auto programObject = MakeShared<ProgramObject>(programId);
        if (programObject == nullptr) return 0;
        programObjects[programId] = programObject;
        return programId;
    }

    const SharedPtr<ProgramObject>& ProgramState::GetProgramObject(const Uint id) {
        auto& programObjects = m_sharedObjectTable ? m_sharedObjectTable->GetProgramObjects() : m_programObjects;
        static SharedPtr<ProgramObject> nullProgramObject = nullptr;
        if (!CheckIndexAvail(id, programObjects)) return nullProgramObject; // FIXME: add error reporting here
        return programObjects[id];
    }

    void ProgramState::MarkProgramObjectForDeletion(const Uint program) {
        auto& programObjects = m_sharedObjectTable ? m_sharedObjectTable->GetProgramObjects() : m_programObjects;
        if (!CheckIndexAvail(program, programObjects)) return; // FIXME: add error reporting here
        auto& programObject = programObjects[program];
        if (programObject != nullptr) {
            programObject->MarkAsDeleted();
            // A program in use is only FLAGGED: its name (and every program query) stays
            // valid until it stops being current, at which point UseProgram finishes the job.
            if (programObject == m_currentProgram) return;
            DestroyProgramSlot(program);
        }
    }

    void ProgramState::DestroyProgramSlot(const Uint program) {
        auto& programObjects = m_sharedObjectTable ? m_sharedObjectTable->GetProgramObjects() : m_programObjects;
        auto& shaderObjects = m_sharedObjectTable ? m_sharedObjectTable->GetShaderObjects() : m_shaderObjects;
        auto& nameGenerator =
            m_sharedObjectTable ? m_sharedObjectTable->GetNameGenerator() : m_programShaderNameGenerator;
        auto& programObject = programObjects[program];
        // P1 join site J4/J5 (glDeleteProgram, and the deferred destroy UseProgram performs
        // when a deletion-flagged program stops being current). The program's name is about
        // to go, so nothing can observe its link any more: cancel-not-join, so a delete never
        // blocks the GL thread on a worker. Explicit rather than left to ~ProgramObject,
        // because the reset below only destroys the object if this table held the last
        // reference - a program still bound as current, or still referenced by a pipeline,
        // outlives it, and its link should stop the moment the name does.
        programObject->CancelLink();
        // Snapshot the attachments: deleting the program is a detach point for shaders
        // that were flagged with glDeleteShader while still attached.
        const Vector<SharedPtr<ShaderObject>> attachedShaders = programObject->GetAttachedShaders();
        programObject.reset();
        nameGenerator.Delete(program);
        for (const auto& shader : attachedShaders) {
            const Uint shaderName = shader->GetExternalIndex();
            if (CheckIndexAvail(shaderName, shaderObjects) && shaderObjects[shaderName] == shader) {
                ReleaseShaderNameIfOrphaned(shaderName);
            }
        }
    }

    Bool ProgramState::ValidateProgramObject(const Uint program) const {
        auto& programObjects = m_sharedObjectTable ? m_sharedObjectTable->GetProgramObjects() : m_programObjects;
        return CheckIndexAvail(program, programObjects) && programObjects[program] != nullptr;
    }

    void ProgramState::UseProgram(Uint program) {
        auto& programObjects = m_sharedObjectTable ? m_sharedObjectTable->GetProgramObjects() : m_programObjects;
        const SharedPtr<ProgramObject> previous = m_currentProgram;

        if (program == 0) m_currentProgram.reset();

        if (CheckIndexAvail(program, programObjects)) {
            m_currentProgram = programObjects[program];
        }

        // A deletion flagged while the program was current takes effect the moment it
        // stops being current.
        if (previous != nullptr && previous != m_currentProgram && previous->GetDeleteStatus()) {
            const Uint previousName = previous->GetExternalIndex();
            if (CheckIndexAvail(previousName, programObjects) && programObjects[previousName] == previous) {
                DestroyProgramSlot(previousName);
            }
        }
    }

    Uint ProgramState::CreateShader(ShaderStage stage) {
        auto& shaderObjects = m_sharedObjectTable ? m_sharedObjectTable->GetShaderObjects() : m_shaderObjects;
        auto& nameGenerator =
            m_sharedObjectTable ? m_sharedObjectTable->GetNameGenerator() : m_programShaderNameGenerator;
        Uint shaderId = 0;
        nameGenerator.Generate(1, &shaderId);
        EnsureIndexAvail(shaderId, shaderObjects);
        auto shaderObject =
            MakeShared<ShaderObject>(stage, shaderId, m_shaderPreprocessCache, m_shaderCompileAdoptionMap);
        if (shaderObject == nullptr) return 0;
        shaderObjects[shaderId] = shaderObject;
        return shaderId;
    }

    const SharedPtr<ShaderObject>& ProgramState::GetShaderObject(const Uint shader) {
        auto& shaderObjects = m_sharedObjectTable ? m_sharedObjectTable->GetShaderObjects() : m_shaderObjects;
        static SharedPtr<ShaderObject> nullShaderObject = nullptr;
        if (!CheckIndexAvail(shader, shaderObjects)) return nullShaderObject;
        return shaderObjects[shader];
    }

    void ProgramState::JoinAllPendingWork() {
        auto& programObjects = m_sharedObjectTable ? m_sharedObjectTable->GetProgramObjects() : m_programObjects;
        auto& shaderObjects = m_sharedObjectTable ? m_sharedObjectTable->GetShaderObjects() : m_shaderObjects;
        // Programs first: a link joins the compiles it depends on, so the shader pass that
        // follows finds most of them already settled. The reverse order would be correct but
        // would wait on each compile twice - once here, once inside the link's own prologue.
        //
        // A copy of each slot rather than a reference into the vector, and an index rather
        // than an iterator: publishing a link replays deferred diagnostics, which reach
        // pGLContext->RecordError. That does not touch these tables today, but it is a sink
        // that can grow, and a reallocation underneath this loop would be a use-after-free
        // that only shows up on the one GL call that walks the whole table. The copy costs a
        // refcount bump on a path a mode switch takes at most once.
        // BOTH phases per program. This is the glMaxShaderCompilerThreadsKHR(0) path, whose
        // contract is that nothing is outstanding when it returns - a program left with its
        // SPIR-V job in flight would make the very next GL_COMPLETION_STATUS_KHR read GL_FALSE
        // in a mode the extension says cannot have anything pending.
        for (SizeT i = 0; i < programObjects.size(); ++i) {
            const SharedPtr<ProgramObject> program = programObjects[i];
            if (program) program->JoinLinkAndSpirv();
        }
        for (SizeT i = 0; i < shaderObjects.size(); ++i) {
            const SharedPtr<ShaderObject> shader = shaderObjects[i];
            if (shader) shader->JoinCompile();
        }
        // The currently-used program is reachable through programObjects unless
        // glDeleteProgram already freed its slot while it stayed current. Nothing else holds
        // a GL-visible name for it, but a draw would still join it, so settle it here too.
        if (m_currentProgram) m_currentProgram->JoinLinkAndSpirv();
    }

    void ProgramState::MarkShaderObjectForDeletion(Uint shader) {
        auto& shaderObjects = m_sharedObjectTable ? m_sharedObjectTable->GetShaderObjects() : m_shaderObjects;
        if (!CheckIndexAvail(shader, shaderObjects)) return;
        auto& shaderObject = shaderObjects[shader];
        if (shaderObject != nullptr) {
            // glDeleteShader on an attached shader only FLAGS it; the name stays valid (and
            // glShaderSource/glCompileShader keep working on it) until the shader is detached
            // from every program. The GL CTS compiles shaders through exactly this
            // create-attach-delete-source-compile sequence (uniform_block.common.name_matching).
            shaderObject->MarkAsDeleted();
            ReleaseShaderNameIfOrphaned(shader);
        }
    }

    Bool ProgramState::ShaderHasGLVisibleAttachment(const SharedPtr<ShaderObject>& shaderObject) const {
        auto& programObjects = m_sharedObjectTable ? m_sharedObjectTable->GetProgramObjects() : m_programObjects;
        for (const auto& programObject : programObjects) {
            if (programObject != nullptr && programObject->ShaderIsAttachedGLVisible(shaderObject)) {
                return true;
            }
        }
        // A program deleted while current vacates its table slot but stays alive as the
        // current program; its attachments still count.
        return m_currentProgram != nullptr && m_currentProgram->ShaderIsAttachedGLVisible(shaderObject);
    }

    void ProgramState::ReleaseShaderNameIfOrphaned(Uint shader) {
        auto& shaderObjects = m_sharedObjectTable ? m_sharedObjectTable->GetShaderObjects() : m_shaderObjects;
        auto& nameGenerator =
            m_sharedObjectTable ? m_sharedObjectTable->GetNameGenerator() : m_programShaderNameGenerator;
        if (!CheckIndexAvail(shader, shaderObjects)) return;
        auto& shaderObject = shaderObjects[shader];
        if (shaderObject == nullptr || !shaderObject->GetDeleteStatus()) return;
        if (ShaderHasGLVisibleAttachment(shaderObject)) return;
        // The name is about to go, so nothing can observe this shader's compile through THIS
        // object any more and a job still in flight for it is pure waste - unless another
        // shader object adopted the same node (stage 6) or a pending link pinned it, which is
        // exactly what ReleaseCompileNode weighs before it cancels anything. Cancel-not-join
        // either way: the job owns its inputs, so dropping the object out from under it is
        // safe and the GL thread never blocks on a delete.
        shaderObject->ReleaseCompileNode();
        shaderObject.reset();
        nameGenerator.Delete(shader);
    }

    Bool ProgramState::ValidateShaderObject(Uint shader) const {
        auto& shaderObjects = m_sharedObjectTable ? m_sharedObjectTable->GetShaderObjects() : m_shaderObjects;
        return CheckIndexAvail(shader, shaderObjects) && shaderObjects[shader] != nullptr;
    }
} // namespace MobileGL::MG_State::GLState

// End of File
