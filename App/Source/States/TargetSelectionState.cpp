#include "States/TargetSelectionState.h"

#include "Combat/TargetResolution.h"
#include "Components/RenderableComponent.h"
#include "Components/SelectedTargetComponent.h"
#include "Components/TabTargetComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Systems/TurnCoordinator.h"

#include <entt/core/hashed_string.hpp>

#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <cstdlib>
#include <optional>

namespace psr {

namespace {

    constexpr const char* kCursorPrefabId = "ui.target_select_cursor";
    constexpr const char* kTravelPreviewPrefabId = "ui.target_travel_preview";
    constexpr const char* kAreaPreviewPrefabId = "ui.target_area_preview";

    // Mirrors KeyBindings.cpp's arrow-key-to-offset mapping -- kept local
    // rather than shared, since MoveAction's own bindings are constructed
    // once as fixed Vec2 offsets baked into each ActionMap entry, not
    // resolved from a raw key code at call time the way this cursor needs.
    std::optional<Vec2> ResolveDirectionKey(int key_code)
    {
        switch (key_code)
        {
        case SDLK_UP:
            return Vec2{0, -1};
        case SDLK_DOWN:
            return Vec2{0, 1};
        case SDLK_LEFT:
            return Vec2{-1, 0};
        case SDLK_RIGHT:
            return Vec2{1, 0};
        default:
            return std::nullopt;
        }
    }

    // Flattens color to a mid-grey of the same alpha -- mirrors
    // UnnamedRoguelike's own TargetSelectionState::Greyed helper, the visual
    // "out of range" signal for the cursor sprite.
    Color Greyed(Color color)
    {
        const auto grey = static_cast<std::uint8_t>((color.r + color.g + color.b) / 3);
        return Color{grey, grey, grey, color.a};
    }

} // namespace

void TargetSelectionState::Begin(TargetRequest request, entt::entity actor)
{
    m_request = request;
    m_actor = actor;
}

void TargetSelectionState::OnEnter(GameplayContext& context)
{
    m_origin = context.registry.GetComponent<Position>(m_actor).tile;
    m_confirmed = false;
    m_cancelled = false;

    // If the actor has a live tab-lock target (TabTargetComponent, see
    // TabTargetSystem.h), start the cursor at/facing it instead of today's
    // plain defaults below -- an out-of-range TargetSquare tile just renders
    // greyed via the usual IsReachable/UpdateCursorVisual path, no special
    // casing needed.
    const TabTargetComponent* tab_target = context.registry.TryGetComponent<TabTargetComponent>(m_actor);
    const Position* target_position = (tab_target && tab_target->target != entt::null)
                                          ? context.registry.TryGetComponent<Position>(tab_target->target)
                                          : nullptr;

    switch (m_request.mode)
    {
    case TargetingMode::SelfTarget:
        m_cursor = m_origin;
        break;
    case TargetingMode::TargetSquare:
        m_cursor = target_position ? target_position->tile : m_origin;
        break;
    case TargetingMode::Directional:
    {
        const Vec2 snapped = target_position ? SnapToCardinalDirection(target_position->tile - m_origin) : Vec2{0, 0};
        m_cursor = snapped == Vec2{0, 0} ? m_origin + Vec2{0, -1} : m_origin + snapped; // default facing: up
        break;
    }
    }

    m_cursor_entity = context.registry.CreateEntity(entt::hashed_string::value(kCursorPrefabId));
    context.grid.AddEntity(m_cursor, m_cursor_entity);

    if (const RenderableComponent* renderable = context.registry.TryGetComponent<RenderableComponent>(m_cursor_entity))
        m_base_color = renderable->color_1;

    UpdateCursorVisual(context);
    UpdatePreview(context);
}

void TargetSelectionState::OnExit(GameplayContext& context)
{
    ClearPreviewEntities(context, m_travel_preview_entities);
    ClearPreviewEntities(context, m_area_preview_entities);

    if (m_cursor_entity == entt::null)
        return;
    context.grid.RemoveEntity(m_cursor, m_cursor_entity);
    context.registry.DestroyEntity(m_cursor_entity);
    m_cursor_entity = entt::null;
}

StateTransition TargetSelectionState::Update(GameplayContext& context, float /*delta_time*/)
{
    if (m_cancelled)
        return StateTransition::Pop();

    if (m_confirmed)
    {
        context.registry.GetOrEmplace<SelectedTargetComponent>(m_actor).tile = m_cursor;
        context.turn_coordinator.SetPendingAction(m_request.action);
        return StateTransition::Pop();
    }

    return StateTransition::None();
}

bool TargetSelectionState::HandleEvent(Event& event, GameplayContext& context)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this, &context](KeyPressedEvent& key_event)
        {
            const int key = key_event.GetKeyCode();
            if (key == SDLK_ESCAPE)
            {
                m_cancelled = true;
                return true;
            }
            if (key == SDLK_SPACE)
            {
                if (IsReachable(m_cursor))
                    m_confirmed = true;
                return true;
            }
            if (std::optional<Vec2> direction = ResolveDirectionKey(key))
            {
                MoveCursor(context, *direction);
                return true;
            }
            return false;
        });
    return event.handled;
}

bool TargetSelectionState::IsReachable(Vec2 tile) const
{
    switch (m_request.mode)
    {
    case TargetingMode::SelfTarget:
        return tile == m_origin;
    case TargetingMode::Directional:
        return true; // whichever cardinal neighbour is currently selected is always a valid pick
    case TargetingMode::TargetSquare:
    {
        const Vec2 delta = tile - m_origin;
        const int chebyshev = std::max(std::abs(delta.x), std::abs(delta.y));
        return chebyshev <= m_request.range;
    }
    }
    return false;
}

void TargetSelectionState::MoveCursor(GameplayContext& context, Vec2 direction)
{
    if (m_request.mode == TargetingMode::SelfTarget)
        return; // fixed at origin

    const Vec2 new_cursor = m_request.mode == TargetingMode::Directional ? m_origin + direction : m_cursor + direction;
    if (!context.grid.Contains(new_cursor))
        return;

    context.grid.RemoveEntity(m_cursor, m_cursor_entity);
    m_cursor = new_cursor;
    context.grid.AddEntity(m_cursor, m_cursor_entity);
    UpdateCursorVisual(context);
    UpdatePreview(context);
}

void TargetSelectionState::UpdateCursorVisual(GameplayContext& context)
{
    RenderableComponent* renderable = context.registry.TryGetComponent<RenderableComponent>(m_cursor_entity);
    if (!renderable)
        return;

    const Color color = IsReachable(m_cursor) ? m_base_color : Greyed(m_base_color);
    renderable->color_1 = color;
    renderable->color_2 = color;
}

void TargetSelectionState::UpdatePreview(GameplayContext& context)
{
    ClearPreviewEntities(context, m_travel_preview_entities);
    ClearPreviewEntities(context, m_area_preview_entities);

    if (!m_request.is_projectile)
        return;

    const Vec2 offset = m_cursor - m_origin;
    if (offset == Vec2{0, 0})
        return;

    const Vec2 direction = SnapToCardinalDirection(offset);
    const std::vector<Vec2> path = BuildProjectilePath(context.grid, context.registry, m_origin, direction,
                                                       m_request.range, m_request.projectile_pierces);
    if (path.empty())
        return;

    SpawnPreviewEntities(context, entt::hashed_string::value(kTravelPreviewPrefabId), path, m_travel_preview_entities);

    const std::vector<Vec2> impact_tiles = m_request.projectile_pierces ? path : std::vector<Vec2>{path.back()};
    SpawnPreviewEntities(context, entt::hashed_string::value(kAreaPreviewPrefabId), impact_tiles,
                         m_area_preview_entities);
}

void TargetSelectionState::ClearPreviewEntities(GameplayContext& context,
                                                std::vector<std::pair<Vec2, entt::entity>>& entities)
{
    for (const auto& [tile, entity] : entities)
    {
        context.grid.RemoveEntity(tile, entity);
        context.registry.DestroyEntity(entity);
    }
    entities.clear();
}

void TargetSelectionState::SpawnPreviewEntities(GameplayContext& context, std::uint32_t prefab_id,
                                                const std::vector<Vec2>& tiles,
                                                std::vector<std::pair<Vec2, entt::entity>>& out_entities)
{
    out_entities.reserve(tiles.size());
    for (Vec2 tile : tiles)
    {
        const entt::entity entity = context.registry.CreateEntity(prefab_id);
        context.grid.AddEntity(tile, entity);
        out_entities.emplace_back(tile, entity);
    }
}

} // namespace psr
