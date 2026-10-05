# Bibliothèque pure C (aucune dépendance HAL) : utilisée par le firmware ET par les tests hôte.
add_library(lps22hh STATIC src/lps22hh.c)
target_include_directories(lps22hh PUBLIC include)
target_compile_options(lps22hh PRIVATE -Wall -Wextra -Wconversion)
