// MobileGL - MobileGL/MG_Impl/GLImpl/Query/GL_Query.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "GL_Query.h"
#include "../Getter/GL_Getter.h"
#include <Config.h>
#include <MG_Backend/BackendObjects.h>
#include <MG_State/GLState/Core.h>
#include <MG_State/GLState/ErrorState/ErrorInfo.h>

namespace MobileGL::MG_Impl::GLImpl {
    namespace {
        using QueryObject = MobileGL::MG_State::GLState::SessionPrivateState::QueryObject;

        MobileGL::MG_State::GLState::SessionPrivateState& SessionQueryState() {
            return MG_State::pGLContext->GetSessionState();
        }

        // Query calls may arrive from any thread (launchers migrate the context
        // across JVM threads); SessionPrivateState owns the registry and the
        // active-slot ids, so each GLContext session is fully isolated.
        QueryObject* FindQueryObjectLocked(GLuint id) {
            return SessionQueryState().FindQuery(id);
        }

        // Whether MobileGL puts GL_ARB_tessellation_shader in its extension string. Read from the
        // ADVERTISED list rather than from a capability bit for the same reason
        // BackendSupportsTextureViews does (GL_Texture.cpp): it makes "MobileGL claims tessellation
        // support" and "the tessellation-conditional API surface is open" the same fact by
        // construction, so the day a backend starts advertising the string the surface below opens
        // with it and no second edit is owed.
        Bool AdvertisesTessellationShaderExtension() {
            const auto& activeBackendObject = MG_Backend::pActiveBackendObject;
            if (!activeBackendObject) return false;
            const auto& extensions = activeBackendObject->GetRendererInfo().RendererGLInfo.Extensions;
            return std::find(extensions.begin(), extensions.end(), E_GL_ARB_tessellation_shader) != extensions.end();
        }

        // The eleven pipeline-statistics counters (GL 4.6 core table 4.3 / ARB_pipeline_statistics_query).
        // A 4.6 core context ACCEPTS the nine unconditional ones at glBeginQuery - there is no query
        // by which an application could learn otherwise before calling. MobileGL instruments none of
        // them, and says so the way GL 4.6 core 4.2.1 provides for: GL_QUERY_COUNTER_BITS answers
        // zero for these targets, which is the spec's own signal that the counter is unsupported and
        // its results indeterminate. That is an honest zero, not an advertised capability - the
        // alternative, GL_INVALID_ENUM on a core entry point, is both non-conformant AND less
        // informative.
        //
        // The two TESSELLATION targets are the exception, because ARB_pipeline_statistics_query
        // makes them conditional on tessellation support rather than unconditional, and the only
        // thing an application (or the conformance suite) can read to decide whether an
        // implementation has it is the GL_ARB_tessellation_shader string. MobileGL does not emit it
        // today, so these two answer GL_INVALID_ENUM: an API surface that accepts a
        // tessellation-conditional token while withholding the string that announces the condition
        // is self-contradictory, and it is the contradiction the suite catches
        // (KHR-GL46.pipeline_statistics_query_tests_ARB.api_coverage_unsupported_calls, whose
        // support probe is gl4cPipelineStatisticsQueryTests.cpp:1166-1176). The gate is the
        // advertisement itself, not a hardcoded "no", so this is one switch and not two.
        Bool IsPipelineStatisticsQueryTarget(GLenum target) {
            switch (target) {
            case GL_VERTICES_SUBMITTED:
            case GL_PRIMITIVES_SUBMITTED:
            case GL_VERTEX_SHADER_INVOCATIONS:
            case GL_GEOMETRY_SHADER_INVOCATIONS:
            case GL_GEOMETRY_SHADER_PRIMITIVES_EMITTED:
            case GL_FRAGMENT_SHADER_INVOCATIONS:
            case GL_COMPUTE_SHADER_INVOCATIONS:
            case GL_CLIPPING_INPUT_PRIMITIVES:
            case GL_CLIPPING_OUTPUT_PRIMITIVES:
                return true;
            case GL_TESS_CONTROL_SHADER_PATCHES:
            case GL_TESS_EVALUATION_SHADER_INVOCATIONS:
                return AdvertisesTessellationShaderExtension();
            default:
                return false;
            }
        }

        Bool TimerQueryDisabled() {
            return MG_Config::Features.DisableTimerQuery;
        }

        void RecordQueryError(ErrorCode code, const char* function, const char* message) {
            MG_State::pGLContext->RecordError(code,
                                              MakeUnique<GenericErrorInfo>("MG_Impl/GLImpl", function, message));
        }

        // The by-buffer query getters write the result into a buffer object instead of client
        // memory. Everything about the query itself - the name, whether it is still active, the
        // parameter - is checked by GetQueryObjectValue; what is left is the destination, so this
        // resolves the buffer and confirms the write lands inside it (GL 4.6 core 4.2.1).
        Bool ResolveQueryResultDestination(GLuint buffer, GLintptr offset, SizeT writeSize, const char* function,
                                           SharedPtr<MG_State::GLState::BufferObject>& outBuffer) {
            if (offset < 0) {
                RecordQueryError(ErrorCode::InvalidValue, function, "Offset cannot be negative.");
                return false;
            }
            if (!MG_State::pGLContext->ValidateBufferObject(buffer)) {
                RecordQueryError(ErrorCode::InvalidOperation, function, "Buffer object does not exist.");
                return false;
            }
            auto bufferObject = MG_State::pGLContext->GetBufferObject(buffer);
            if (!bufferObject) {
                RecordQueryError(ErrorCode::InvalidOperation, function, "Buffer object does not exist.");
                return false;
            }
            if (static_cast<SizeT>(offset) + writeSize > bufferObject->GetSize()) {
                RecordQueryError(ErrorCode::InvalidOperation, function,
                                 "The query result does not fit in the buffer object at this offset.");
                return false;
            }
            outBuffer = bufferObject;
            return true;
        }

        // Callers must hold the session state's registry.
        void ResetQueryObjectLocked(QueryObject* queryObject) {
            if (queryObject->backendHandle) {
                if (const auto deleteBackendQuery = MG_Backend::gBackendFunctionsTable.GL.DeleteBackendQuery) {
                    deleteBackendQuery(queryObject->backendHandle);
                }
                queryObject->backendHandle = nullptr;
            }
            queryObject->active = false;
            queryObject->ended = false;
            queryObject->resultCached = false;
            queryObject->cachedResult = 0;
        }

        // Session state owns its registry; callers hold no extra scope lock.
        void EndTimeElapsedQueryLocked(QueryObject* queryObject) {
            const auto endTimeElapsedQuery = MG_Backend::gBackendFunctionsTable.GL.EndTimeElapsedQuery;
            if (endTimeElapsedQuery && queryObject->backendHandle) {
                endTimeElapsedQuery(queryObject->backendHandle);
            }
            queryObject->active = false;
            queryObject->ended = true;
            SessionQueryState().activeTimeElapsedQueryId = 0;
        }

        // The CPU accounting counter a transform feedback query target reads: what the capture
        // buffers took for GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, and everything the capture
        // stage assembled - a paused span included - for GL_PRIMITIVES_GENERATED. One counter
        // for both targets would report the clamped written count as the generated one.
        Uint64 TransformFeedbackCounterForTarget(GLenum target) {
            return target == GL_PRIMITIVES_GENERATED
                       ? MG_State::pGLContext->GetTransformFeedbackGeneratedCounter()
                       : MG_State::pGLContext->GetTransformFeedbackPrimitiveCounter();
        }

        // The span's CPU accounting delta. Saturating: a snapshot left above its counter (a
        // context switch between Begin and End, a counter that never moved) would otherwise
        // wrap to 2^64-1, which GetQueryObjectuiv hands the app as 4294967295.
        Uint64 TransformFeedbackCpuResult(const QueryObject* queryObject) {
            const Uint64 counter = TransformFeedbackCounterForTarget(queryObject->target);
            return counter > queryObject->counterSnapshot ? counter - queryObject->counterSnapshot : 0;
        }

        // Whether this ended span's result should come from the CPU accounting rather than from
        // the backend query it also ran. Three conditions, all necessary:
        //   * the backend asked for it (DirectGLES, whose ES driver counter is the unreliable
        //     one; DirectVulkan never sets the bit and so is untouched by any of this);
        //   * the target is PRIMITIVES_WRITTEN. GL_PRIMITIVES_GENERATED counts primitives
        //     whether or not a capture is active, and the accounting only ever sees capture
        //     draws, so the backend's counter is the more complete answer there;
        //   * the span was fully accounted: at least one capture draw reached the accounting
        //     (the instanced, indirect and multi-draw entry points do not call it at all, so a
        //     span made of those is invisible to it) and none of them amplified through a
        //     geometry stage, which the CPU cannot model.
        Bool PrefersCpuTransformFeedbackResult(const QueryObject* queryObject) {
            if (!MG_Backend::gBackendFunctionsTable.GL.PrefersCpuXfbPrimitiveAccounting) return false;
            if (queryObject->target != GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) return false;
            if (MG_State::pGLContext->GetTransformFeedbackGeometryCaptureDraws() !=
                queryObject->geometryCaptureDrawSnapshot) {
                return false;
            }
            return MG_State::pGLContext->GetTransformFeedbackAccountedCaptureDraws() !=
                   queryObject->accountedCaptureDrawSnapshot;
        }

        // Shared GetQueryObject* implementation. Returns false when an error
        // was recorded and no value should be written back. `outValueProduced`, when given,
        // additionally distinguishes "succeeded with a value" from "succeeded but the result is not
        // ready" - the GL_QUERY_RESULT_NO_WAIT case, where GL_ARB_query_buffer_object says the
        // destination is left alone rather than written with a placeholder.
        Bool GetQueryObjectValue(GLuint id, GLenum pname, const char* function, Uint64& outValue,
                                 Bool* outValueProduced = nullptr) {
            if (outValueProduced) *outValueProduced = true;
            // Session state owns its own registry and active slots; no extra scope lock.
            auto* queryObject = FindQueryObjectLocked(id);
            if (!queryObject) {
                RecordQueryError(ErrorCode::InvalidOperation, function, "Query object does not exist.");
                return false;
            }
            if (queryObject->active) {
                RecordQueryError(ErrorCode::InvalidOperation, function, "Query object is still active.");
                return false;
            }

            switch (pname) {
            case GL_QUERY_TARGET:
                // The target a query was begun with (or created with, for glCreateQueries) - state
                // the object has carried all along, GL 4.6 core table 23.35.
                outValue = queryObject->target;
                return true;
            case GL_QUERY_RESULT_NO_WAIT: {
                if (queryObject->resultCached) {
                    outValue = queryObject->cachedResult;
                    return true;
                }
                Uint64 result = 0;
                const auto getQueryResult64 = MG_Backend::gBackendFunctionsTable.GL.GetQueryResult64;
                if (queryObject->backendHandle && getQueryResult64 &&
                    !getQueryResult64(queryObject->backendHandle, /*wait=*/false, &result)) {
                    // Not ready. The whole point of the no-wait form is that the caller's
                    // destination keeps whatever it already held.
                    if (outValueProduced) *outValueProduced = false;
                    outValue = 0;
                    return true;
                }
                if (queryObject->target == GL_ANY_SAMPLES_PASSED ||
                    queryObject->target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE) {
                    result = result != 0 ? 1 : 0;
                }
                if (queryObject->backendHandle) {
                    if (const auto deleteBackendQuery = MG_Backend::gBackendFunctionsTable.GL.DeleteBackendQuery) {
                        deleteBackendQuery(queryObject->backendHandle);
                    }
                    queryObject->backendHandle = nullptr;
                }
                queryObject->cachedResult = result;
                queryObject->resultCached = true;
                outValue = result;
                return true;
            }
            case GL_QUERY_RESULT_AVAILABLE: {
                if (queryObject->resultCached || !queryObject->backendHandle) {
                    outValue = 1;
                    return true;
                }
                const auto isQueryResultAvailable = MG_Backend::gBackendFunctionsTable.GL.IsQueryResultAvailable;
                outValue = (!isQueryResultAvailable || isQueryResultAvailable(queryObject->backendHandle)) ? 1 : 0;
                return true;
            }
            case GL_QUERY_RESULT: {
                if (queryObject->resultCached) {
                    outValue = queryObject->cachedResult;
                    return true;
                }
                Uint64 result = 0;
                if (queryObject->backendHandle) {
                    const auto getQueryResult64 = MG_Backend::gBackendFunctionsTable.GL.GetQueryResult64;
                    if (getQueryResult64 &&
                        !getQueryResult64(queryObject->backendHandle, /*wait=*/true, &result)) {
                        // The backend could not produce the result YET (e.g. a
                        // Vulkan wait refusing to block on a not-yet-submitted
                        // frame serial). Per the documented no-stall tradeoff
                        // this call reads 0, but the value is NOT cached and
                        // the backend handle is kept, so a later AVAILABLE
                        // poll / RESULT read still produces the real value.
                        outValue = 0;
                        return true;
                    }
                    // ANY_SAMPLES_PASSED* report a boolean.
                    if (queryObject->target == GL_ANY_SAMPLES_PASSED ||
                        queryObject->target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE) {
                        result = result != 0 ? 1 : 0;
                    }
                    // Final value produced (or no GetQueryResult64 hook: the
                    // query degrades to a zero result); the backend handle is
                    // consumed and the value cached for later reads.
                    if (const auto deleteBackendQuery = MG_Backend::gBackendFunctionsTable.GL.DeleteBackendQuery) {
                        deleteBackendQuery(queryObject->backendHandle);
                    }
                    queryObject->backendHandle = nullptr;
                }
                queryObject->cachedResult = result;
                queryObject->resultCached = true;
                outValue = result;
                return true;
            }
            default:
                RecordQueryError(ErrorCode::InvalidEnum, function, "Unsupported query object parameter.");
                return false;
            }
        }

        template <typename T>
        void GetQueryBufferObject(GLuint id, GLuint buffer, GLenum pname, GLintptr offset, const char* function) {
            SharedPtr<MG_State::GLState::BufferObject> bufferObject;
            if (!ResolveQueryResultDestination(buffer, offset, sizeof(T), function, bufferObject)) return;

            Uint64 value = 0;
            Bool valueProduced = false;
            if (!GetQueryObjectValue(id, pname, function, value, &valueProduced)) return;
            // GL_QUERY_RESULT_NO_WAIT on a result that has not landed writes nothing at all.
            if (!valueProduced) return;

            const T narrowed = static_cast<T>(value);
            bufferObject->UploadSubData({const_cast<T*>(&narrowed), sizeof(T)}, static_cast<SizeT>(offset));
        }
    } // namespace

    void GenQueries(GLsizei n, GLuint* ids) {
        if (n < 0) {
            RecordQueryError(ErrorCode::InvalidValue, __FUNCTION__, "n cannot be negative.");
            return;
        }
        if (!ids) {
            return;
        }
        // Session state owns its own registry and active slots; no extra scope lock.
        auto& state = SessionQueryState();
        for (GLsizei i = 0; i < n; ++i) {
            const GLuint id = state.NextQueryId();
            (void)state.AllocateQuery(id);
            ids[i] = id;
        }
    }

    // glCreateQueries differs from glGenQueries in creating the objects outright, with their
    // target already fixed and the rest of their state at the defaults (GL 4.6 core 4.2.1).
    void CreateQueries(GLenum target, GLsizei n, GLuint* ids) {
        switch (target) {
        case GL_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED:
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        case GL_TIME_ELAPSED:
        case GL_TIMESTAMP:
        case GL_PRIMITIVES_GENERATED:
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
            break;
        default:
            RecordQueryError(ErrorCode::InvalidEnum, __FUNCTION__, "Query target is not accepted.");
            return;
        }
        if (n < 0) {
            RecordQueryError(ErrorCode::InvalidValue, __FUNCTION__, "n cannot be negative.");
            return;
        }
        if (!ids) {
            return;
        }
        // Session state owns its own registry and active slots; no extra scope lock.
        auto& state = SessionQueryState();
        for (GLsizei i = 0; i < n; ++i) {
            const GLuint id = state.NextQueryId();
            auto& queryObject = state.AllocateQuery(id);
            queryObject.target = target;
            queryObject.created = true;
            ids[i] = id;
        }
    }

    void DeleteQueries(GLsizei n, const GLuint* ids) {
        if (n < 0) {
            RecordQueryError(ErrorCode::InvalidValue, __FUNCTION__, "n cannot be negative.");
            return;
        }
        if (!ids) {
            return;
        }
        // Session state owns its own registry and active slots; no extra scope lock.
        auto& state = SessionQueryState();
        for (GLsizei i = 0; i < n; ++i) {
            QueryObject* queryObject = state.FindQuery(ids[i]);
            if (queryObject == nullptr) {
                continue; // unknown ids are silently ignored
            }
            if (queryObject->active) {
                // Implicitly end before deletion, releasing the matching active slot.
                if (queryObject->target == GL_SAMPLES_PASSED || queryObject->target == GL_ANY_SAMPLES_PASSED ||
                    queryObject->target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE) {
                    if (const auto endOcclusionQuery = MG_Backend::gBackendFunctionsTable.GL.EndOcclusionQuery;
                        endOcclusionQuery && queryObject->backendHandle) {
                        endOcclusionQuery(queryObject->backendHandle);
                    }
                    queryObject->active = false;
                    state.activeSamplesPassedQueryId = 0;
                } else if (IsPipelineStatisticsQueryTarget(queryObject->target)) {
                    queryObject->active = false;
                    state.activePipelineStatisticsQueryIds[queryObject->target] = 0;
                } else if (queryObject->target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN ||
                           queryObject->target == GL_PRIMITIVES_GENERATED) {
                    queryObject->active = false;
                    (queryObject->target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN
                         ? state.activePrimitivesWrittenQueryId
                         : state.activePrimitivesGeneratedQueryId) = 0;
                } else {
                    EndTimeElapsedQueryLocked(queryObject);
                }
            }
            if (queryObject->backendHandle) {
                if (const auto deleteBackendQuery = MG_Backend::gBackendFunctionsTable.GL.DeleteBackendQuery) {
                    deleteBackendQuery(queryObject->backendHandle);
                }
                queryObject->backendHandle = nullptr;
            }
            state.EraseQuery(ids[i]);
        }
    }

    GLboolean IsQuery(GLuint id) {
        if (id == 0) {
            return GL_FALSE;
        }
        // Session state owns its own registry and active slots; no extra scope lock.
        // A name from glGenQueries is not yet a query object: it becomes one when it is first
        // used with BeginQuery/QueryCounter (which is what a non-zero target records), or
        // immediately if it came from glCreateQueries.
        const auto* queryObject = FindQueryObjectLocked(id);
        return (queryObject != nullptr && (queryObject->created || queryObject->target != 0)) ? GL_TRUE : GL_FALSE;
    }

    void BeginQuery(GLenum target, GLuint id) {
        const Bool isTransformFeedbackQuery =
            target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN || target == GL_PRIMITIVES_GENERATED;
        const Bool isOcclusionQuery =
            (target == GL_SAMPLES_PASSED || target == GL_ANY_SAMPLES_PASSED ||
             target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE) &&
            MG_Backend::gBackendFunctionsTable.GL.BeginOcclusionQuery != nullptr;
        const Bool isPipelineStatisticsQuery = IsPipelineStatisticsQueryTarget(target);
        if (target != GL_TIME_ELAPSED && !isTransformFeedbackQuery && !isOcclusionQuery &&
            !isPipelineStatisticsQuery) {
            // GL_TIMESTAMP is not a valid BeginQuery target; the occlusion targets
            // need backend support.
            RecordQueryError(ErrorCode::InvalidEnum, __FUNCTION__, "Query target is not supported.");
            return;
        }
        if (id == 0) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "Query id 0 cannot be used.");
            return;
        }
        // Session state owns its own registry and active slots; no extra scope lock.
        auto& state = SessionQueryState();
        auto* queryObject = FindQueryObjectLocked(id);
        if (!queryObject) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "Query object does not exist.");
            return;
        }
        GLuint& activeQueryId = isPipelineStatisticsQuery
            ? state.activePipelineStatisticsQueryIds[target]
            : (isTransformFeedbackQuery
                   ? (target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN ? state.activePrimitivesWrittenQueryId
                                                                         : state.activePrimitivesGeneratedQueryId)
                   : (isOcclusionQuery ? state.activeSamplesPassedQueryId : state.activeTimeElapsedQueryId));
        if (activeQueryId != 0) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__,
                             "A query is already active on this target.");
            return;
        }
        if (queryObject->active) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "Query object is already active.");
            return;
        }
        if (queryObject->target != 0 && queryObject->target != target) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__,
                             "Query object was already used with a different target.");
            return;
        }

        ResetQueryObjectLocked(queryObject); // discard any previous result
        queryObject->target = target;
        queryObject->active = true;
        if (isPipelineStatisticsQuery) {
            // Nothing to start: the counter is uninstrumented and GL_QUERY_COUNTER_BITS says so.
            // The object still becomes a real, target-latched query so every other rule about it
            // (re-use with another target, double-begin, EndQuery pairing) keeps holding.
        } else if (isTransformFeedbackQuery) {
            // Prefer real GPU transform-feedback queries (exact with geometry shaders);
            // the CPU accounting delta stays as the fallback when the backend lacks them.
            const auto beginXfbPrimitivesQuery = MG_Backend::gBackendFunctionsTable.GL.BeginXfbPrimitivesQuery;
            queryObject->backendHandle =
                beginXfbPrimitivesQuery ? beginXfbPrimitivesQuery(target == GL_PRIMITIVES_GENERATED) : nullptr;
            queryObject->counterSnapshot = TransformFeedbackCounterForTarget(target);
            queryObject->accountedCaptureDrawSnapshot =
                MG_State::pGLContext->GetTransformFeedbackAccountedCaptureDraws();
            queryObject->geometryCaptureDrawSnapshot =
                MG_State::pGLContext->GetTransformFeedbackGeometryCaptureDraws();
        } else if (isOcclusionQuery) {
            queryObject->backendHandle = MG_Backend::gBackendFunctionsTable.GL.BeginOcclusionQuery();
        } else {
            const auto beginTimeElapsedQuery = MG_Backend::gBackendFunctionsTable.GL.BeginTimeElapsedQuery;
            queryObject->backendHandle =
                (!TimerQueryDisabled() && beginTimeElapsedQuery) ? beginTimeElapsedQuery() : nullptr;
        }
        activeQueryId = id;
    }

    void EndQuery(GLenum target) {
        const Bool isTransformFeedbackQuery =
            target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN || target == GL_PRIMITIVES_GENERATED;
        const Bool isOcclusionQuery =
            (target == GL_SAMPLES_PASSED || target == GL_ANY_SAMPLES_PASSED ||
             target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE) &&
            MG_Backend::gBackendFunctionsTable.GL.BeginOcclusionQuery != nullptr;
        const Bool isPipelineStatisticsQuery = IsPipelineStatisticsQueryTarget(target);
        if (target != GL_TIME_ELAPSED && !isTransformFeedbackQuery && !isOcclusionQuery &&
            !isPipelineStatisticsQuery) {
            RecordQueryError(ErrorCode::InvalidEnum, __FUNCTION__, "Query target is not supported.");
            return;
        }
        // Session state owns its own registry and active slots; no extra scope lock.
        auto& state = SessionQueryState();
        GLuint& activeQueryId = isPipelineStatisticsQuery
            ? state.activePipelineStatisticsQueryIds[target]
            : (isTransformFeedbackQuery
                   ? (target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN ? state.activePrimitivesWrittenQueryId
                                                                         : state.activePrimitivesGeneratedQueryId)
                   : (isOcclusionQuery ? state.activeSamplesPassedQueryId : state.activeTimeElapsedQueryId));
        if (activeQueryId == 0) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "No query is active on this target.");
            return;
        }
        auto* queryObject = FindQueryObjectLocked(activeQueryId);
        if (!queryObject) {
            activeQueryId = 0; // should not happen; keep state consistent
            return;
        }
        if (isPipelineStatisticsQuery) {
            // The result is a definite zero rather than an unread backend handle, so a later
            // GetQueryObject* answers immediately and never waits on something that was never
            // started. GL_QUERY_COUNTER_BITS = 0 is what marks that zero indeterminate.
            queryObject->cachedResult = 0;
            queryObject->resultCached = true;
            queryObject->active = false;
            queryObject->ended = true;
            activeQueryId = 0;
            return;
        }
        if (isTransformFeedbackQuery) {
            if (queryObject->backendHandle) {
                if (const auto endXfbPrimitivesQuery = MG_Backend::gBackendFunctionsTable.GL.EndXfbPrimitivesQuery) {
                    endXfbPrimitivesQuery(queryObject->backendHandle);
                }
            }
            // A backend query that is not going to be read is released here, not left to be
            // collected later: the span is over, the driver object has nothing left to say.
            // Ending it first is what makes that legal.
            if (!queryObject->backendHandle || PrefersCpuTransformFeedbackResult(queryObject)) {
                if (queryObject->backendHandle) {
                    if (const auto deleteBackendQuery = MG_Backend::gBackendFunctionsTable.GL.DeleteBackendQuery) {
                        deleteBackendQuery(queryObject->backendHandle);
                    }
                    queryObject->backendHandle = nullptr;
                }
                queryObject->cachedResult = TransformFeedbackCpuResult(queryObject);
                queryObject->resultCached = true;
            }
            // Otherwise the result comes from the GPU query at read time.
            queryObject->active = false;
            queryObject->ended = true;
            activeQueryId = 0;
            return;
        }
        if (isOcclusionQuery) {
            if (const auto endOcclusionQuery = MG_Backend::gBackendFunctionsTable.GL.EndOcclusionQuery;
                endOcclusionQuery && queryObject->backendHandle) {
                endOcclusionQuery(queryObject->backendHandle);
            }
            queryObject->active = false;
            queryObject->ended = true;
            activeQueryId = 0;
            return;
        }
        EndTimeElapsedQueryLocked(queryObject);
    }

    void QueryCounter(GLuint id, GLenum target) {
        if (target != GL_TIMESTAMP) {
            RecordQueryError(ErrorCode::InvalidEnum, __FUNCTION__, "QueryCounter target must be GL_TIMESTAMP.");
            return;
        }
        if (id == 0) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "Query id 0 cannot be used.");
            return;
        }
        // Session state owns its own registry and active slots; no extra scope lock.
        auto* queryObject = FindQueryObjectLocked(id);
        if (!queryObject) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "Query object does not exist.");
            return;
        }
        if (queryObject->active) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "Query object is currently active.");
            return;
        }
        if (queryObject->target != 0 && queryObject->target != target) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__,
                             "Query object was already used with a different target.");
            return;
        }

        ResetQueryObjectLocked(queryObject); // discard any previous result
        queryObject->target = target;
        const auto queryCounterTimestamp = MG_Backend::gBackendFunctionsTable.GL.QueryCounterTimestamp;
        queryObject->backendHandle =
            (!TimerQueryDisabled() && queryCounterTimestamp) ? queryCounterTimestamp() : nullptr;
        queryObject->ended = true;
    }

    void BeginConditionalRender(GLuint id, GLenum mode) {
        // GL 4.6 core 10.9's eight modes. The _INVERTED half flips the sense of the predicate;
        // the BY_REGION half only narrows WHERE an implementation is permitted to discard, so
        // treating it as its whole-framebuffer sibling is what an implementation without region
        // granularity does. The _NO_WAIT half is a permission to render rather than stall, not an
        // obligation - see the resolve below.
        Bool inverted = false;
        switch (mode) {
        case GL_QUERY_WAIT:
        case GL_QUERY_NO_WAIT:
        case GL_QUERY_BY_REGION_WAIT:
        case GL_QUERY_BY_REGION_NO_WAIT:
            inverted = false;
            break;
        case GL_QUERY_WAIT_INVERTED:
        case GL_QUERY_NO_WAIT_INVERTED:
        case GL_QUERY_BY_REGION_WAIT_INVERTED:
        case GL_QUERY_BY_REGION_NO_WAIT_INVERTED:
            inverted = true;
            break;
        default:
            RecordQueryError(ErrorCode::InvalidEnum, __FUNCTION__, "mode is not a conditional render mode.");
            return;
        }

        if (MG_State::pGLContext->IsConditionalRenderActive()) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "Conditional rendering is already active.");
            return;
        }

        {
            // Session state owns its own registry and active slots; no extra scope lock.
            const auto* queryObject = FindQueryObjectLocked(id);
            // A generated NAME is not yet a query object; it becomes one at its first use with a
            // target (the same rule glIsQuery answers by).
            if (!queryObject || (!queryObject->created && queryObject->target == 0)) {
                RecordQueryError(ErrorCode::InvalidValue, __FUNCTION__, "id is not the name of a query object.");
                return;
            }
            if (queryObject->active) {
                RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "The query object is still active.");
                return;
            }
            if (queryObject->target != GL_SAMPLES_PASSED && queryObject->target != GL_ANY_SAMPLES_PASSED &&
                queryObject->target != GL_ANY_SAMPLES_PASSED_CONSERVATIVE) {
                RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__,
                                 "Conditional rendering requires an occlusion query object.");
                return;
            }
        }

        // Resolved ONCE, here, and by WAITING even for the _NO_WAIT modes: the spec lets those
        // render instead of stalling, so always waiting is conforming and is the only choice that
        // gives the whole block one deterministic verdict. Reading it per command instead would
        // let a result that lands mid-block change the answer half way through.
        Uint64 samplesPassed = 0;
        if (!GetQueryObjectValue(id, GL_QUERY_RESULT, __FUNCTION__, samplesPassed)) return;
        const Bool passed = samplesPassed != 0;
        MG_State::pGLContext->BeginConditionalRender(id, mode, inverted ? passed : !passed);
    }

    void EndConditionalRender() {
        if (!MG_State::pGLContext->IsConditionalRenderActive()) {
            RecordQueryError(ErrorCode::InvalidOperation, __FUNCTION__, "Conditional rendering is not active.");
            return;
        }
        MG_State::pGLContext->EndConditionalRender();
    }

    void GetQueryiv(GLenum target, GLenum pname, GLint* params) {
        if (!params) {
            return;
        }
        switch (pname) {
        case GL_CURRENT_QUERY: {
            // Session state owns its own registry and active slots; no extra scope lock.
            auto& state = SessionQueryState();
            switch (target) {
            case GL_TIME_ELAPSED:
                *params = static_cast<GLint>(state.activeTimeElapsedQueryId);
                break;
            case GL_SAMPLES_PASSED:
            case GL_ANY_SAMPLES_PASSED:
            case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
                *params = static_cast<GLint>(state.activeSamplesPassedQueryId);
                break;
            case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
                *params = static_cast<GLint>(state.activePrimitivesWrittenQueryId);
                break;
            case GL_PRIMITIVES_GENERATED:
                *params = static_cast<GLint>(state.activePrimitivesGeneratedQueryId);
                break;
            default:
                if (IsPipelineStatisticsQueryTarget(target)) {
                    const auto it = state.activePipelineStatisticsQueryIds.find(target);
                    *params = it != state.activePipelineStatisticsQueryIds.end() ? static_cast<GLint>(it->second) : 0;
                } else {
                    *params = 0;
                }
                break;
            }
            return;
        }
        case GL_QUERY_COUNTER_BITS: {
            // 64 bits are advertised only while the live backend can actually
            // time: IsTimerQuerySupported is the dynamic truth (extension /
            // entry points / timestamp valid bits at call time, not at table
            // init), and the MOBILEGL_DISABLE_TIMERQUERY kill switch always
            // wins.
            if (IsPipelineStatisticsQueryTarget(target)) {
                // Zero: GL 4.6 core 4.2.1's way of saying the counter is not implemented and its
                // results are indeterminate. The conformance suite reads exactly this and skips
                // the functional half of each such target, which is the outcome an uninstrumented
                // counter should produce.
                *params = 0;
                return;
            }
            if (target == GL_SAMPLES_PASSED || target == GL_ANY_SAMPLES_PASSED ||
                target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE) {
                const Bool occlusionSupported = MG_Backend::gBackendFunctionsTable.GL.BeginOcclusionQuery != nullptr;
                *params = occlusionSupported ? (target == GL_SAMPLES_PASSED ? 32 : 1) : 0;
                return;
            }
            const Bool timerTarget = target == GL_TIME_ELAPSED || target == GL_TIMESTAMP;
            const auto isTimerQuerySupported = MG_Backend::gBackendFunctionsTable.GL.IsTimerQuerySupported;
            const Bool supported =
                timerTarget && !TimerQueryDisabled() && isTimerQuerySupported && isTimerQuerySupported();
            *params = supported ? 64 : 0;
            return;
        }
        default:
            RecordQueryError(ErrorCode::InvalidEnum, __FUNCTION__, "Unsupported query parameter.");
            return;
        }
    }

    void GetQueryBufferObjectiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
        GetQueryBufferObject<GLint>(id, buffer, pname, offset, __FUNCTION__);
    }

    void GetQueryBufferObjectuiv(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
        GetQueryBufferObject<GLuint>(id, buffer, pname, offset, __FUNCTION__);
    }

    void GetQueryBufferObjecti64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
        GetQueryBufferObject<GLint64>(id, buffer, pname, offset, __FUNCTION__);
    }

    void GetQueryBufferObjectui64v(GLuint id, GLuint buffer, GLenum pname, GLintptr offset) {
        GetQueryBufferObject<GLuint64>(id, buffer, pname, offset, __FUNCTION__);
    }

    void GetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
        Uint64 value = 0;
        Bool valueProduced = false;
        if (!GetQueryObjectValue(id, pname, __FUNCTION__, value, &valueProduced) || !valueProduced || !params) {
            return;
        }
        constexpr Uint64 kMaxInt = static_cast<Uint64>(INT_MAX);
        *params = value > kMaxInt ? INT_MAX : static_cast<GLint>(value);
    }

    void GetQueryObjectuiv(GLuint id, GLenum pname, GLuint* params) {
        Uint64 value = 0;
        Bool valueProduced = false;
        if (!GetQueryObjectValue(id, pname, __FUNCTION__, value, &valueProduced) || !valueProduced || !params) {
            return;
        }
        *params = static_cast<GLuint>(value & 0xFFFFFFFFull);
    }

    void GetQueryObjecti64v(GLuint id, GLenum pname, GLint64* params) {
        Uint64 value = 0;
        Bool valueProduced = false;
        if (!GetQueryObjectValue(id, pname, __FUNCTION__, value, &valueProduced) || !valueProduced || !params) {
            return;
        }
        *params = static_cast<GLint64>(value);
    }

    void GetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params) {
        Uint64 value = 0;
        Bool valueProduced = false;
        if (!GetQueryObjectValue(id, pname, __FUNCTION__, value, &valueProduced) || !valueProduced || !params) {
            return;
        }
        *params = static_cast<GLuint64>(value);
    }

    namespace {
        Bool IsPerVertexStreamQueryTarget(GLenum target) {
            return target == GL_PRIMITIVES_GENERATED || target == GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN;
        }

        // The indexed query entry points differ from the plain ones only in the vertex
        // stream they address (GL 4.6 core 4.2.1): index must be below GL_MAX_VERTEX_STREAMS
        // for the two transform feedback targets and zero for every other target. MobileGL
        // implements ONE vertex stream, so both bounds are 1 and a valid call is always index 0 -
        // which is what makes the three forwards below equivalent to the unindexed entry points.
        //
        // THAT EQUIVALENCE IS THE WHOLE JUSTIFICATION, and it is read out of the getter rather
        // than assumed: the moment GL_MAX_VERTEX_STREAMS answers more than one, index 1..3 starts
        // reaching EndQueryIndexed and GetQueryIndexediv, which resolve the active query from
        // per-TARGET globals and would end - or report - a query begun on a different stream.
        // Raising that limit therefore means giving each active query a stream index and
        // comparing it here, not just changing the number.
        Bool ValidateQueryStreamIndex(const char* function, GLenum target, GLuint index) {
            const Bool perStreamTarget = IsPerVertexStreamQueryTarget(target);
            GLint maxVertexStreams = 1;
            if (perStreamTarget) {
                GetIntegerv(GL_MAX_VERTEX_STREAMS, &maxVertexStreams);
            }
            if (index < static_cast<GLuint>(std::max(maxVertexStreams, 1))) {
                return true;
            }
            RecordQueryError(ErrorCode::InvalidValue, function,
                             perStreamTarget ? "index is not less than GL_MAX_VERTEX_STREAMS."
                                             : "index must be zero for this query target.");
            return false;
        }

    } // namespace

    void BeginQueryIndexed(GLenum target, GLuint index, GLuint id) {
        if (!ValidateQueryStreamIndex(__FUNCTION__, target, index)) return;
        BeginQuery(target, id);
    }

    void EndQueryIndexed(GLenum target, GLuint index) {
        if (!ValidateQueryStreamIndex(__FUNCTION__, target, index)) return;
        EndQuery(target);
    }

    void GetQueryIndexediv(GLenum target, GLuint index, GLenum pname, GLint* params) {
        if (!ValidateQueryStreamIndex(__FUNCTION__, target, index)) return;
        GetQueryiv(target, pname, params);
    }

    void DestroyAllQueryObjects() {
        // Sweep the current session's session-private registry. Without this
        // drain, every query the app left undeleted survived full library
        // teardown in the process-global registry.
        auto& state = SessionQueryState();
        const auto ids = state.GetQueryIds();
        if (ids.empty()) {
            return;
        }
        // Backend handles must be released by the backend that created them, so
        // this runs while the function table is still populated. Both backends'
        // DeleteBackendQuery are generation-guarded, so a handle whose renderer
        // or ES context is already gone frees only the wrapper.
        const auto deleteBackendQuery = MG_Backend::gBackendFunctionsTable.GL.DeleteBackendQuery;
        for (GLuint id : ids) {
            auto* queryObject = state.FindQuery(id);
            if (queryObject != nullptr) {
                if (deleteBackendQuery && queryObject->backendHandle) {
                    deleteBackendQuery(queryObject->backendHandle);
                }
                state.EraseQuery(id);
            }
        }
        state.activeTimeElapsedQueryId = 0;
        state.activePrimitivesWrittenQueryId = 0;
        state.activePrimitivesGeneratedQueryId = 0;
        state.activeSamplesPassedQueryId = 0;
        state.activePipelineStatisticsQueryIds.clear();
        MGLOG_D("DestroyAllQueryObjects: reclaimed %zu query object(s) the app left undeleted", ids.size());
    }
} // namespace MobileGL::MG_Impl::GLImpl
