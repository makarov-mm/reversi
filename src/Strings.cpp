#include "Strings.h"

#include <array>
#include <cstddef>
#include <utility>

namespace {

using Pair = std::pair<std::string_view, std::string_view>;

constexpr std::array<Pair, static_cast<std::size_t>(Str::Count)> Table{{
    {"3D Reversi", "Реверси 3D"},
    {"Black to move", "Ход чёрных"},
    {"White to move", "Ход белых"},
    {"Computer is thinking…", "Компьютер думает…"},
    {"Draw", "Ничья"},
    {"Black wins", "Чёрные победили"},
    {"White wins", "Белые победили"},
    {"the board is full", "доска заполнена"},
    {"neither side can move", "ходов больше нет"},
    {"Black has no move and passes", "Чёрным некуда ходить — пас"},
    {"White has no move and passes", "Белым некуда ходить — пас"},
    {"pass", "пас"},
    {"You", "Вы"},
    {"Computer", "Компьютер"},
    {"Player", "Игрок"},
    {"Black", "Чёрные"},
    {"White", "Белые"},
    {"Moves", "Ходы"},
    {"No moves yet", "Ходов пока нет"},
    {"Undo", "Назад"},
    {"Redo", "Вперёд"},
    {"Hint", "Подсказка"},
    {"New game", "Новая партия"},
    {"Flip board", "Развернуть"},
    {"Mode", "Режим"},
    {"You play Black", "Вы за чёрных"},
    {"You play White", "Вы за белых"},
    {"Two players", "Два игрока"},
    {"Demo game", "Демонстрация"},
    {"Level", "Уровень"},
    {"Beginner", "Новичок"},
    {"Casual", "Любитель"},
    {"Club player", "Разрядник"},
    {"Expert", "Сильный"},
    {"Master", "Мастер"},
    {"Show moves", "Показ ходов"},
    {"Theme", "Тема"},
    {"Sound", "Звук"},
    {"On", "Вкл"},
    {"Off", "Выкл"},
    {"English", "Русский"},
    {"F1 help · Tab panel · F11 full screen", "F1 справка · Tab панель · F11 весь экран"},
    {"Controls", "Управление"},
    {"Click", "Щелчок"},
    {"Drag", "Перетаскивание"},
    {"Wheel", "Колесо мыши"},
    {"Place a disc on a marked square", "Поставить фишку на отмеченное поле"},
    {"Turn the view (either mouse button)", "Повернуть вид (любой кнопкой мыши)"},
    {"Zoom in and out", "Приблизить или отдалить"},
    {"Take back", "Отменить ход"},
    {"Replay", "Вернуть ход"},
    {"Suggest a move", "Подсказать ход"},
    {"New game", "Новая партия"},
    {"Flip the board", "Развернуть доску"},
    {"View from above", "Вид сверху"},
    {"Computer level", "Уровень компьютера"},
    {"Who plays whom", "Кто с кем играет"},
    {"Show or hide the legal moves", "Показать/скрыть возможные ходы"},
    {"Next theme", "Следующая тема"},
    {"Sound on or off", "Звук вкл/выкл"},
    {"English / Russian", "Русский / английский"},
    {"Show or hide the panel", "Показать/скрыть панель"},
    {"Full screen", "Весь экран"},
    {"Copy the game, Shift: the position", "Копировать партию, Shift: позицию"},
    {"Paste a game or a position", "Вставить партию или позицию"},
    {"Close this help", "Закрыть справку"},
    {"Rules", "Правила"},
    {"Place a disc so that a line of your opponent's discs lies between it and one of yours: they all turn to your color. Black begins; a side that cannot move passes. The most discs at the end wins.",
     "Поставьте фишку так, чтобы между ней и вашей фишкой оказался ряд фишек соперника, — они перевернутся в ваш цвет. Начинают чёрные; кому некуда ходить, пропускает ход. Побеждает тот, у кого в конце больше фишек."},
    {"Game copied", "Партия скопирована"},
    {"Position copied", "Позиция скопирована"},
    {"Game loaded", "Партия загружена"},
    {"The clipboard has no game or position", "В буфере нет партии или позиции"},
    {"Hint: ", "Подсказка: "},
    {"No moves to take back", "Нечего отменять"},
    {"Nothing to turn over there", "Сюда нельзя: нечего переворачивать"},
}};

} // namespace

std::string_view Tr(Str id, Language language)
{
    const Pair& pair = Table[std::to_underlying(id)];
    return language == Language::Russian ? pair.second : pair.first;
}
