// gcc_gimple_json_plugin.cpp – плагин GCC выводящий GIMPLE/IR в формате JSON
// Версия 1.1.0 – обход базовых блоков выполняется в глубину (рекурсивно)
// Сборка: g++ -shared -fPIC -std=c++11 $(gcc -print-file-name=plugin)/include/gcc-plugin.h ...
// -----------------------------------------------------------------------------

// Подключение основных заголовочных файлов для разработки плагина GCC
#include <gcc-plugin.h>            // Основные определения для разработки плагинов GCC
#include <plugin-version.h>        // Проверка совместимости версии плагина с версией GCC

#include <config.h>                // Общие конфигурационные параметры сборки
#include <system.h>                // Системные вызовы и утилиты
#include <coretypes.h>             // Основные типы дерева (tree)
#include <tm.h>                    // Информация о модели машинного кода и целевой архитектуре
#include <tree.h>                  // Операции с AST (abstract syntax tree)

#include <tree-pass.h>             // Интерфейс для создания проходов по дереву
#include <gimple.h>                // Работа с промежуточным представлением GIMPLE
#include <basic-block.h>           // Структура базовых блоков (CFG)
#include <context.h>               // Контекст компиляции (gcc::context)
#include <gimple-iterator.h>       // Итераторы для обхода GIMPLE-инструкций
#include <cfgloop.h>               // Анализ циклов в графе потока управления (CFG)

#include <sstream>                 // Для формирования строкового вывода JSON
#include <cstdlib>                 // Стандартная библиотека C (malloc, free и т.д.)
#include <cstdio>                  // Функции C ввода-вывода (printf, sprintf)
#include <unordered_set>           // Контейнер для хранения множества посещённых блоков

// Пометка о совместимости лицензии GPL
int plugin_is_GPL_compatible = 1;

// Определение имени, версии и помощи для плагина
#define PLUGIN_NAME    "my-plugin-json"
#define PLUGIN_VERSION "1.1.0"
#define PLUGIN_HELP    "Плагин выводит GIMPLE/IR в формате JSON с рекурсивным обходом CFG."

// Информация о плагине (используется в обратных вызовах PLUGIN_INFO)
static struct plugin_info my_plugin_info = {
    .version = PLUGIN_VERSION,
    .help    = PLUGIN_HELP
};

//------------------------------------------------------------------------------
// Метаданные нашего прохода (pass)
//------------------------------------------------------------------------------
static const struct pass_data my_pass_data = {
    GIMPLE_PASS,              /* type: тип прохода – GIMPLE-проход */
    PLUGIN_NAME,              /* name: имя прохода */
    OPTGROUP_NONE,            /* optinfo_flags: без группы опций */
    TV_NONE,                  /* time_var_id: не отслеживаем время */
    PROP_gimple_any,          /* properties_required: требуемые свойства IR */
    0,                        /* properties_provided: обеспечиваемых свойств нет */
    0,                        /* properties_destroyed: не уничтожаем свойства */
    0,                        /* todo_flags_start: флаги до начала */
    0                         /* todo_flags_finish: флаги после завершения */
};

// Поток для сбора JSON и флаг, используемый при выводе первой функции
static std::ostringstream json_os;
static bool first_function = true;

//------------------------------------------------------------------------------
// Небольшие вспомогательные функции
//------------------------------------------------------------------------------

// Функция экранирует специальные символы в строках для JSON
std::string json_escape(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;  // кавычка
            case '\\': out += "\\\\"; break;  // обратный слэш
            case '\b': out += "\\b";  break;      // backspace
            case '\f': out += "\\f";  break;      // formfeed
            case '\n': out += "\\n";  break;      // newline
            case '\r': out += "\\r";  break;      // carriage return
            case '\t': out += "\\t";  break;      // tab
            default:
                if (c < 0x20) {
                    char buf[7];
                    sprintf(buf, "\\u%04x", c);  // юникод экранирование
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

// Получение человекочитаемого имени элемента AST (tree)
std::string json_get_name(tree t) {
    if (!t) return "NULL";

    switch (TREE_CODE(t)) {
        case VAR_DECL:
        case PARM_DECL:
        case FUNCTION_DECL:
            if (DECL_NAME(t))
                return IDENTIFIER_POINTER(DECL_NAME(t));  // имя переменной или функции
            return "<unnamed>";
        case INTEGER_CST: {
            char buf[32];
            sprintf(buf, "%ld", TREE_INT_CST_LOW(t)); // константа целого
            return buf;
        }
        case SSA_NAME: {
            // Для SSA-промежуточных имен включаем версию
            tree var = SSA_NAME_VAR(t);
            std::string varname = json_get_name(var);
            char buf[32];
            sprintf(buf, "%u", SSA_NAME_VERSION(t));
            return varname + "_" + buf;
        }
        default:
            // Для прочих кодов выводим их имя в угловых скобках
            return std::string("<") + get_tree_code_name(TREE_CODE(t)) + ">";
    }
}

// Возвращает строковое представление оператора для заданного кода tree_code
const char* get_operator_name(enum tree_code code) {
    switch (code) {
        case PLUS_EXPR:      return "+";
        case MINUS_EXPR:     return "-";
        case MULT_EXPR:      return "*";
        case TRUNC_DIV_EXPR:
        case FLOOR_DIV_EXPR:
        case CEIL_DIV_EXPR:  return "/";
        case LT_EXPR:        return "<";
        case GT_EXPR:        return ">";
        case LE_EXPR:        return "<=";
        case GE_EXPR:        return ">=";
        case EQ_EXPR:        return "==";
        case NE_EXPR:        return "!=";
        default:             return "?";  // неизвестный оператор
    }
}

// FIXED
// Преобразование дерева выражений в строковое представление (с учётом унарных/бинарных операций)
std::string tree_to_string(tree t) {
    if (!t) return "NULL";

    if (TREE_CODE(t) == INTEGER_CST) {
        char buf[32];
        sprintf(buf, "%ld", TREE_INT_CST_LOW(t));
        return buf;
    }

    if (TREE_CODE(t) == SSA_NAME) {
        return json_get_name(t);
    }

    if (TREE_CODE(t) == VAR_DECL || TREE_CODE(t) == PARM_DECL) {
        return json_get_name(t);
    }

    // Рекурсивно обрабатываем бинарные и сравнительные выражения
    if (TREE_CODE_CLASS(TREE_CODE(t)) == tcc_binary ||
        TREE_CODE_CLASS(TREE_CODE(t)) == tcc_comparison) {

        tree lhs = TREE_OPERAND(t, 0);
        tree rhs = TREE_OPERAND(t, 1);
        return "(" + tree_to_string(lhs) + " " +
               get_operator_name(TREE_CODE(t)) + " " +
               tree_to_string(rhs) + ")";
    }

    // Унарные операции (например, отрицание)
    if (TREE_CODE_CLASS(TREE_CODE(t)) == tcc_unary) {
        tree op = TREE_OPERAND(t, 0);
        return get_operator_name(TREE_CODE(t)) + tree_to_string(op);
    }

    // Прочие узлы дерева выводим в виде <CODE>
    return std::string("<") + get_tree_code_name(TREE_CODE(t)) + ">";
}

// Преобразование GIMPLE-инструкции в строковое представление для JSON
std::string gimple_stmt_to_string(gimple *stmt) {
    std::ostringstream oss;

    switch (gimple_code(stmt)) {
        case GIMPLE_ASSIGN: {
            tree lhs = gimple_assign_lhs(stmt);
            tree rhs1 = gimple_assign_rhs1(stmt);
            tree rhs2 = gimple_assign_rhs2(stmt);

            // Формируем присваивание: lhs = rhs1 или lhs = rhs1 op rhs2
            oss << json_get_name(lhs) << " = ";
            if (rhs2) {
                oss << tree_to_string(rhs1) << " "
                    << get_operator_name(gimple_assign_rhs_code(stmt)) << " "
                    << tree_to_string(rhs2);
            } else {
                oss << tree_to_string(rhs1);
            }
            break;
        }
        case GIMPLE_COND: {
            // Условная ветвящая инструкция: if (lhs op rhs)
            oss << "if (" << tree_to_string(gimple_cond_lhs(stmt)) << " "
                << get_operator_name(gimple_cond_code(stmt)) << " "
                << tree_to_string(gimple_cond_rhs(stmt)) << ")";
            break;
        }
        case GIMPLE_PHI: {
            // Phi-функция SSA: result = PHI(args...)
            tree result = gimple_phi_result(stmt);
            oss << json_get_name(result) << " = PHI(";
            for (unsigned i = 0, n = gimple_phi_num_args(stmt); i < n; ++i) {
                if (i) oss << ", ";
                oss << json_get_name(gimple_phi_arg_def(stmt, i));
            }
            oss << ")";
            break;
        }
        default:
            // Для прочих типов инструкций выводим их код
            oss << get_tree_code_name(static_cast<enum tree_code>(gimple_code(stmt)));
    }
    // Экранируем результат для корректного JSON
    return json_escape(oss.str());
}

//------------------------------------------------------------------------------
// Обход CFG в глубину – форвардное объявление функции
//------------------------------------------------------------------------------
static void traverse_bb(basic_block bb, std::unordered_set<int> &visited,
                        function *fn, bool &first_bb);

//------------------------------------------------------------------------------
// Вывод одного базового блока в JSON
//------------------------------------------------------------------------------
static void print_bb_json(basic_block bb, bool &first_bb) {
    if (!first_bb)
        json_os << ",";  // разделитель между блоками
    else
        first_bb = false;

    json_os << "\n      {";
    json_os << "\n        \"index\": " << bb->index << ",";

    // Список предшественников (index исходных блоков)
    json_os << "\n        \"predecessors\": [";
    bool first_pred = true;
    for (edge e : bb->preds) {
        if (!first_pred) json_os << ","; else first_pred = false;
        json_os << e->src->index;
    }
    json_os << "],";

    // Список преемников (index целевых блоков)
    json_os << "\n        \"successors\": [";
    bool first_succ = true;
    for (edge e : bb->succs) {
        if (!first_succ) json_os << ","; else first_succ = false;
        json_os << e->dest->index;
    }
    json_os << "],";

    // Список инструкций внутри блока
    json_os << "\n        \"statements\": [";
    bool first_stmt = true;
    for (gimple_stmt_iterator gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
        gimple *stmt = gsi_stmt(gsi);

        if (!first_stmt) json_os << ","; else first_stmt = false;

        std::string stmt_type;
        switch (gimple_code(stmt)) {
            case GIMPLE_PHI:   stmt_type = "Phi-функция"; break;
            case GIMPLE_COND:  stmt_type = "Инструкция ветвления"; break;
            case GIMPLE_ASSIGN: {
                tree rhs = gimple_assign_rhs1(stmt);
                switch (TREE_CODE(rhs)) {
                    case ARRAY_REF: stmt_type = "ArrayRef"; break;
                    case PLUS_EXPR:
                    case MINUS_EXPR:
                    case MULT_EXPR:
                    case TRUNC_DIV_EXPR:
                    case FLOOR_DIV_EXPR:
                    case CEIL_DIV_EXPR:
                        stmt_type = "Арифметическая операция"; break;
                    default:
                        stmt_type = "Присваивание";
                }
                break;
            }
            default:
                stmt_type = "Инструкция";
        }

        json_os << "\n          {";
        json_os << "\n            \"type\": \"" << json_escape(stmt_type) << "\",";
        json_os << "\n            \"representation\": \"" << gimple_stmt_to_string(stmt) << "\"";
        json_os << "\n          }";
    }
    json_os << "\n        ]";
    json_os << "\n      }";
}

//------------------------------------------------------------------------------
// Рекурсивный DFS-поиск по CFG
//------------------------------------------------------------------------------

// FIXED
static void traverse_bb(basic_block bb, std::unordered_set<int> &visited,
                        function *fn, bool &first_bb) {
    if (!bb || visited.count(bb->index)) return;  // пропустить, если уже посещён
    visited.insert(bb->index);

    print_bb_json(bb, first_bb);

    // Рекурсивно обойти все преемники
    for (edge e : bb->succs) {
        traverse_bb(e->dest, visited, fn, first_bb);
    }
}

//------------------------------------------------------------------------------
// Реализация нашего прохода GIMPLE-плагина
//------------------------------------------------------------------------------
struct my_pass : gimple_opt_pass {
    // Конструктор: инициализируем базовый класс
    my_pass(gcc::context *ctx) : gimple_opt_pass(my_pass_data, ctx) {}

    // Основная функция прохода – вызывается для каждой функции (AST function)
    unsigned int execute(function *fn) override {
        if (!first_function) json_os << ","; else first_function = false;

        // Начало JSON-объекта для функции
        json_os << "\n  {";
        json_os << "\n    \"name\": \"" << json_escape(json_get_name(fn->decl)) << "\",";
        json_os << "\n    \"basic_blocks\": [";

        std::unordered_set<int> visited;  // множество посещённых блоков
        bool first_bb = true;              // флаг первого блока

        // Начать обход с преемников ENTRY-блока (пропустить искусственный ENTRY)
        basic_block entry = ENTRY_BLOCK_PTR_FOR_FN(fn);
        for (edge e : entry->succs) {
            traverse_bb(e->dest, visited, fn, first_bb); // FIXED
        }

        // Закрываем массив блоков и объект функции
        json_os << "\n    ]";
        json_os << "\n  }";
        return 0;
    }

    // Клонирование прохода (не используется, возвращаем тот же объект)
    my_pass *clone() override { return this; }
};

//------------------------------------------------------------------------------
// Регистрация прохода и обратных вызовов плагина
//------------------------------------------------------------------------------
static struct register_pass_info my_pass_info = {
    new my_pass(g),          /* pass: указатель на наш проход */
    "ssa",                  /* reference_pass_name: вставляем после SSA-прохода */
    1,                       /* insert_after: позиция вставки */
    PASS_POS_INSERT_AFTER    /* PASS_POS_INSERT_AFTER: после указанного */
};

// Callback при начале единицы компиляции: инициализируем JSON
static void plugin_start_unit(void *, void *) {
    json_os.str("");
    json_os.clear();
    json_os << "{\n  \"functions\": [";
    first_function = true;
}

// Callback при завершении плагина: выводим накопленный JSON в stdout
static void plugin_finish(void *, void *) {
    json_os << "\n  ]\n}\n";
    printf("%s\n", json_os.str().c_str());  // вывод в консоль для перенаправления
}

// Точка входа плагина: проверяем версию GCC и регистрируем обратные вызовы
int plugin_init(struct plugin_name_args *plugin_info,
                struct plugin_gcc_version *version) {
    if (!plugin_default_version_check(version, &gcc_version)) {
        fprintf(stderr, "Неподходящая версия GCC\n");
        return 1;  // ошибка инициализации при несовместимости версий
    }

    // Регистрируем информацию о плагине
    register_callback(plugin_info->base_name, PLUGIN_INFO, nullptr, &my_plugin_info);
    // При старте единицы компиляции
    register_callback(plugin_info->base_name, PLUGIN_START_UNIT, plugin_start_unit, nullptr);
    // При завершении компиляции
    register_callback(plugin_info->base_name, PLUGIN_FINISH, plugin_finish, nullptr);
    // Настройка менеджера проходов – вставляем наш проход
    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, nullptr, &my_pass_info);

    return 0;  // успешная инициализация плагина
}
