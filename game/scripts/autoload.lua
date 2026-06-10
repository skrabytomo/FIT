-- autoload.lua — loaded at game start
-- Define trigger callbacks here or load other scripts

function onEnterTown(ctx)
    game.print("Hero " .. ctx.heroId .. " entered a town")
end

function onBattleWon(ctx)
    game.print("Victory! Hero " .. ctx.heroId)
end

function onBattleLost(ctx)
    game.print("Defeat — retreating")
end

function onWeekStart(ctx)
    game.print("Week " .. game.getWeek() .. " begins")
end

game.print("Scripts loaded — " .. game.version())
