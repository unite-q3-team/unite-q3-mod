### Unite Q3 Mod — документация

Этот документ описывает команды, переменные консоли (cvar'ы) и ключевые подсистемы мода. Везде, где указывается «(auth)», команда доступна только авторизованным игрокам; где «(cheats)» — требует включённых читов `sv_cheats`.

Ссылки на исходники приведены по ключевым местам в дереве:
- Клиентские команды: `code/game/g_cmds.c`
- Консольные (серверные) команды: `code/game/g_svcmds.c`
- Голосовалка: `code/game/cmds/votesystem.c`
- Редактор предметов: `code/game/cmds/itemreplace.c`
- Инструменты позиций/спавнов/игроков: `code/game/cmds/pos.c`, `code/game/cmds/player.c`
- Ротация карт: `code/game/g_rotation.c`
- Базовые квар'ы: `code/game/g_cvar.h`
- Антилэг (unlagged): `code/game/g_unlagged.c`


### Команды игроков (клиентские)

- Базовый чат/инфо:
  - `say`, `say_team`, `tell <id> <msg>` — чат всем/команде/лично
  - `score` — стандартная табличка очков
  - `scores` — расширенная сводка по игрокам (K/D, dmg, FPH)
  - `where` — вывести свои координаты
  - `time` / `servertime` — текущее локальное/серверное время
  - `sysinfo` — сводка по серверу (tickrate, аптайм, сущности)

- Команды наблюдения/команды:
  - `team [red|blue|spectator]` — смена команды (c кулдауном `g_teamChangeCooldown`)
  - `follow <id>` / `follownext` / `followprev` — следование за игроками
  - `teamtask <t>` — выставление «тимтаска» в userinfo

- Голосования:
  - `callvote <cmd> [arg]` — вызвать голосование (см. раздел «Голосовалка»)
  - `vote <yes|no>` — проголосовать
  - `cv [map]` — справка/шорткат по голосовалке, вывод списка команд

- Статы/награды:
  - `stats [clientNum]` — подробная статистика по оружию и урону
  - `statsall` — вывести статистику пачками по всем игрокам
  - `topshots` — лучшие точности по видам оружия
  - `awards` — сводка «наград матча» (most kills, best eff, streak, и т.д.)

- Навигация/позиции (см. `cmds/pos.c`):
  - `getpos` — вывести свои координаты
  - `setpos <x> <y> <z>` — телепорт себя; либо `setpos <id>` к игроку; см. справку команды для вариантов
  - `poscp` (auth) — вкл/выкл CenterPrint координат (в т.ч. при follow)

- Управление спавнами (auth, см. `cmds/player.c`):
  - `spawns` — показать маркеры точек спавна
  - `spawnadd` / `spawnrm` — добавить/удалить ближайший спавн
  - `spawnsave` — сохранить точки в `spawns.txt`
  - `spawnreload` — перезагрузить `spawns.txt` (нужен `g_customSpawns 1`)
  - `spawnundo` / `spawnredo` — откат/повтор последней правки
  - `spawnang [yaw [pitch]]` — задать углы ближайшему спавну (или скопировать с вида)
  - `spawnscp` — CenterPrint инфо по ближайшему спавну

- Утилиты игроков (auth, см. `cmds/player.c` и `cmds/misc.c`):
  - `playerlist` / `players` — расширенный список игроков (rate/snaps/мод/страна)
  - `from` — таблица стран игроков (GeoIP)
  - `killplayer <id>` — убить игрока (не зрителя)
  - `fteam <id> <red|blue|spectator|free>` — мягкий перевод в команду
  - `shuffle <random|score>` — перемешать команды (см. также серверную `shuffle`)

- Редактор предметов (auth, см. `cmds/itemreplace.c`):
  - `items` — список всех типов предметов
  - `listitems` — список исходных предметов на карте (с позицией/углами)
  - `irreload` — перезагрузка правил из `itemreplace.txt`
  - `irlist [map]` — вывести применимые к текущей карте правила/класскарты/добавления
  - `irmatchcp` — включить CenterPrint ближайшего совпадения правил
  - `iradd <classname> [yaw]` — заспавнить предмет у ног
  - `irrm [radius]` — удалить ближайший предмет
  - `irrepl <toClass> [radius]` — заменить ближайший предмет
  - `irmove [x y z]` — сдвинуть ближайший предмет (или к себе)
  - `irundo` / `irredo` — undo/redo правок предметов
  - `irrespawn` — переспавнить предметы согласно исходному списку + правилам/добавлениям
  - `irsave` — дописать сделанные правки в `itemreplace.txt`

- Крюк (free grappling hook):
  - `+hook` / `-hook` — выстрел/отпустить крюк (зависит от `g_hook`)

- Прочее:
  - `help` — список доступных команд (отмечает отключённые DC'ом; auth-команды выводятся отдельно)
  - `rotation` — показать текущую ротацию карт из файла `g_rotation`
  - `maplist [page]` — список карт на сервере постранично
  - `ready` / `unready` — система готовности на разминке (`g_doReady`/warmup)

- Команды только при `sv_cheats 1`:
  - `give [all|weapons|ammo|armor|health|...]`
  - `god`, `notarget`, `noclip`, `levelshot`, `setviewpos x y z yaw`


### Команды консоли сервера

- Администрирование/служебные (`code/game/g_svcmds.c`):
  - `entitylist` — список сущностей
  - `forceteam <player> <team>` — насильно перевести игрока
  - `game_memory` — сводка по аллокатору
  - `addbot`, `botlist`, `abort_podium` — стандартные Q3 команды ботов/подия
  - `addip <mask>`, `removeip <mask>`, `listip` — фильтрация по IP (см. `g_banIPs`, `g_filterBan`)
  - `rotate [index]` — перейти к следующей карте в файле ротации (см. «Ротация»)
  - `shuffle <random|score>` — серверное перераспределение команд по режиму
  - (test) — удалена
  - `playerlist` / `players` — серверный аналог списка игроков
  - `say ...` — вещание от имени сервера


### Голосовалка (votesystem)

Гибкая система голосований конфигурируется через `votesystem.txt` в корне мода. При отсутствии создаётся дефолт с примерами.

- Базовые голосования по умолчанию: `map`, `g_gametype`, `nextmap (= rotate)` и алиасы-конфиги (`instagib`, `freeze`, `noquad`, `timelimit`, `fraglimit`, `capturelimit`, `shuffle`).
- Правила в `votesystem.txt` задаются как пары `ключ = значение` с пространством имён по имени правила. Поля:
  - `<name>.enabled = 0|1`
  - `<name>.name = <отображаемое название>`
  - `<name>.action = <шаблон команды>; {arg} подставляется из аргумента`
  - `<name>.usage`, `<name>.requireArg = 0|1`, `<name>.numericOnly = 0|1`
  - `<name>.cvar = <имя cvar>` — для показа текущего значения в списке
  - `<name>.case.<ключ> = <action>` — ветвление по строковым значениям
  - `<name>.default = <action>` — действие по умолчанию

Контроль cvar'ами:
- `g_allowVote` (1) — разрешить голосования
- `g_allowVoteChange` (1) — разрешить менять голос
- `g_voteLimit` (3) — лимит вызовов голосования на карту
- `g_teamVoteLimit` (3) — лимит командных голосований
- Дополнительно регистрируются: `g_voteTime` (30 сек), `g_voteExecuteDelayMs` (3000 мс)


### Редактор предметов (itemreplace)

Файл `itemreplace.txt` описывает класс-карты и точечные правила замены предметов на уровне. При отсутствии создаётся дефолт с примерами.

- Класс-карта:
  - `map.<MAP>.classmap.<fromClass> = <toClass>` — заменить все `<fromClass>` на `<toClass>`
- Точечные правила:
  - `map.<MAP>.rule.<ID>.match.classname = item_quad`
  - `map.<MAP>.rule.<ID>.match.origin = X Y Z` (опц.), `match.tolerance = 32`
  - `map.<MAP>.rule.<ID>.apply.classname = ...`, `apply.origin = ...`, `apply.angles = P Y R` или `apply.angle = Y`, `apply.spawnflags = N`, `apply.remove = 1`
- Перезапуск предметов: `irrespawn`
- Сохранение текущих правок: `irsave`

Тумблер системы: `g_itemReplace 1`


### Ротация карт

- Квар `g_rotation` — путь к файлу ротации (список карт и блоки настроек). Команда консоли `rotate` переключает на следующую карту; индекс хранится в `SV_ROTATION`.
- Парсер ротации умеет присваивать cvar'ы: строки вида `$cvar = value` применяются при входе в блок.


### Freeze Tag (режим заморозки)

Ключевые квар'ы (`g_cvar.h`):
- `g_freeze` (serverinfo|userinfo|latch) — включить режим
- `g_thawTime` — время разморозки напарника
- `g_thawTimeAutoRevive` — автоворожд-е через N сек; отдельные лимиты на лаву/«за карту»/телепорт: `g_thawTimeAuto_lava|_bounds|_tp`
- `g_thawRadius` — радиус разморозки
- `g_freeze_forceRevive`, `g_freeze_beginFrozen`, `g_freeze_beginFrozenDelay`
- Поведение наблюдения в тим-режимах: `g_teamAllowEnemySpectate`, `g_autoFollowTeammate`, `g_teamNoFreeSpectate`

Спец. учтены переходы между командами, follow/ready, списание голосов при смене команды и т.п.


### Unlagged (лаг-компенсация)

- `g_unlagged 1` — полная компенсация выстрелов по прошлым позициям противника
- `g_unlagged_ShiftClientNew 1` — альтернативный алгоритм сдвига клиентов
- `g_predictPVS 0/1` — предсказание видимости (опционально)
Реализация: `G_TimeShiftAllClients`, `G_DoTimeShiftFor`, откат позиций `G_UnTimeShift*` (`code/game/g_unlagged.c`).


### Крюк (grappling hook)

Квар'ы:
- `g_hook 1` — включить серверный крюк
- `g_hook_speed`, `g_hook_pullSpeed`, `g_hook_maxDist`, `g_hook_visiblePull`, `g_hook_allowPlayers`
Команды: `+hook`/`-hook`.


### Расширенные статы/награды

Ведётся покадровый и пооружейный учёт попаданий/урона/подборов, на базе которого работают:
- `stats`, `statsall`, `topshots`, `awards` (см. `g_cmds.c`)


### Прочие подсистемы

- Система готовности (`ready`/`unready`) во время warmup. Включает автостарт при готовности всех людей.
- Расширенные списки игроков/GeoIP (нужен `geoip.dat` в корне мода).
- Перемешивание команд `shuffle <random|score>` с балансом по сумме очков в режиме `score`.


### Важные cvar'ы (по группам)

Базовые серверные:
- `g_gametype`, `sv_maxclients`, `g_maxGameClients`, `dmflags`, `fraglimit`, `timelimit`, `capturelimit`
- `g_friendlyFire`, `g_teamForceBalance`, `g_autoJoin`
- `g_warmup`, `g_log`, `g_logSync`, `g_password`, `g_needpass`, `dedicated`
- `sv_fps` — тикрейт сервера

Движение/сетевое:
- `pmove_fixed`, `pmove_msec`, `sv_pmove_fixed`, `sv_pmove_msec`, `g_smoothClients`
- Devotion: `pmove_autohop`, `pmove_accurate`

Стартовые наборы/урон/скорости:
- `g_startWeapon`, `g_startWeapons`, `g_start_health`, `g_start_armor`
- `g_*_start_ammo` для каждого оружия
- `g_*_damage`, `g_*_fireRatio`
- `g_rl_projectileSpeed`, `g_pg_projectileSpeed`, `g_bfg_projectileSpeed`

Особые режимы/мутаторы:
- `g_vampire`, `g_vampire_max_health`
- `g_instagib`
- `g_railJump`
- `g_items_deathDrop`, `g_dropWeaponOnDeath`, `g_dropPowerupsOnDeath`
- Спавны/предметы: `g_spawn_items`, `g_spawn_weapons`, `g_spawn_quad`, `g_spawnProtectTime`

Хоминг/рикошеты:
- Тумблеры: `g_homing_rl|pg|bfg|gl`
- Радиусы: `g_homing_*_radius`
- «Умность»: `g_homing_*_smart`
- Рикошеты: `g_ricochet_gl|rl|pg|bfg` (‑1 — безлимит, 0 — выкл, N — лимит)

Антилэг/заморозка/наблюдение — см. соответствующие разделы.

XQ3E фичи/индикаторы:
- `g_x_drawDamage`, `g_x_unfreezeFoe`, `g_x_drawHitbox` — публикуются через configstring (см. `UpdateXQ3EFeaturesConfigString`)

Админ/отладка:
- `g_teamChangeCooldown` — КД на смену команды
- `g_debug*` (move/damage/alloc/freeze)
- `dbg_events` — лог «Kill:/Item:»

Голосования — см. выше (включая `g_voteTime`, `g_voteExecuteDelayMs`).

FreezeTag — см. выше.

Крюк — см. выше.


### Боты и их cvar'ы

Множество настроек ИИ регистрируются в `ai_dmq3.c`/`ai_main.c` (префикс `bot_...`), среди них:
- Передвижение/уклонения: `bot_dodge*`, `bot_predictobstacles`, дистанции для LG
- Прицел/точности: `bot_accuracy_scale_*`, `bot_aim*`, «wiggle»‑параметры
- Поведение: `bot_rocketjump`, `bot_grapple`, `bot_fastchat`, `bot_nochat`, `bot_challenge` и пр.

Для подбора значений ориентируйтесь на комментарии в исходниках `code/game/ai_*.c`.


### Управление доступом к командам

Часть команд помечены «требуют авторизацию» (см. `ent->authed`). Список доступных отображается в `help`; отключённые админом команды подсвечиваются. Некоторые команды возможно отключать централизованно (DC_IsDisabled/«disabled commands»).


### Примечания по безопасности

- Голосовалка и парсеры конфигов фильтруют «опасные» символы (`;`, переводы строк) из входа.
- Админские команды предметов/спавнов доступны только авторизованным.


### Быстрые шпаргалки

- Показать доступные голосования: `\cv`
- Включить instagib через голосование: `\callvote instagib 1`
- Включить FreezeTag через голосование: `\callvote freeze 1`
- Перемешать команды по счёту: `\shuffle score` (или через голосование `\callvote shuffle score` при наличии правила)
- Список карт: `\maplist [страница]`
- Показать ротацию: `\rotation`


### Файлы данных

- `votesystem.txt` — правила голосований (создаётся с дефолтным содержимым при первом запуске)
- `itemreplace.txt` — правила замены предметов (создаётся с дефолтом)
- `spawns.txt` — сохранённые спавны пользовательского редактора
- `geoip.dat` — база стран для GeoIP (опц.)


### Известные cvar'ы по умолчанию (сервисы)

- `g_doWarmup` принудительно регистрируется ROM=1 и выставляется в 1 (см. `g_main.c`).


### Версии/совместимость

Мод рассчитан на Q3 1.32; учтены различия ванильных бинарей при рестарте карты (`G_LoadMap`).


