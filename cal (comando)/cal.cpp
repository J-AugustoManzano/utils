/*
 * Unix "cal" for Windows
 *   C:> g++ cal.cpp -o cal
 *
 * Autor do original em "C": Christopher Wellons
 * https://github.com/skeeto/scratch/blob/master/windows/cal.c
 *
 * Traducao e adaptação para "C++": Augusto Manzano (AM-42)
 * Esta versao esta ajustada para operar com os catamanhodarios
 * Juliano e Gregoriano com base no ano de 1582.
 *
 * Versao preliminar (em desenvolvimento)
 */

#include <iostream>
#include <windows.h>

#define VERSION "1.0.0"

const int BUFMAX = 2048; // apenas no pior caso

struct dataModo {
  int ano, mes;
  wchar_t dias[7][2];
  wchar_t meses[12][15];
  int tamanho[12];
};

struct arg {
  wchar_t *beg, *end;
};

/* Recuperar os nomes dos meses e as abreviacoes dos
 * dias da semana para a configuração regional do usuario
 * do Win32. Os meses serao truncados para 15 caracteres.
 * Os dias possuem exatamente 2 caracteres de comprimento
 * e sao preenchidos com espacos, se necessario.
 */
 static void os_IniciaDataModo(dataModo *dm) {
  int i, j;
  SYSTEMTIME st;
  GetSystemTime(&st);
  dm->ano = st.wYear;
  dm->mes = st.wMonth;
  for (i = 0; i < 12; i++) {
    wchar_t tmp[80];
    int n = GetLocaleInfoW(0x400, 56 + i, tmp, 80) - 1;
    dm->tamanho[i] = n = n > 15 ? 15 : n;
    for (j = 0; j < n; j++) {
      dm->meses[i][j] = tmp[j];
    }
  }
  for (i = 0; i < 7; i++) {
    wchar_t tmp[9];
    int n = GetLocaleInfoW(0x400, 49 + (i + 6) % 7, tmp, 9) - 1;
    dm->dias[i][0] = n > 0 ? tmp[0] : ' ';
    dm->dias[i][1] = n > 1 ? tmp[1] : ' ';
  }
}

// Escreve sequencia Unicode na saida padrao
static int os_EscreveUnicode(int fd, const wchar_t *memo, int tamanho) {
  int i;
  HANDLE h;
  DWORD tmp;
  int utamanho = 0;
  char u8[BUFMAX * 2]; // muito alem do pior caso

  h = GetStdHandle(fd == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
  if (GetConsoleMode(h, &tmp)) {
    return WriteConsoleW(h, memo, tamanho, &tmp, 0) and (int)tmp == tamanho;
  }

  // Converte para padrao UTF-8
  for (i = 0; i < tamanho; i++) {
    wchar_t r = memo[i];
    if (r < 0x80) {
      u8[utamanho++] = r;
    } else if (r < 0x800) {
      u8[utamanho++] = 0xc0 | (r >> 6);
      u8[utamanho++] = 0x80 | (r >> 0 & 63);
    } else {
      u8[utamanho++] = 0xe0 | (r >> 12);
      u8[utamanho++] = 0x80 | (r >> 6 & 63);
      u8[utamanho++] = 0x80 | (r >> 0 & 63);
    }
  }
  return WriteFile(h, u8, utamanho, &tmp, 0) and (int)tmp == utamanho;
}


/* Retorna o proximo argumento da linha de comando dado o
 * argumento anterior. Inicialize com zero na primeira chamada.
 * O final é exclusivo e os ponteiros serao nulos quando nao
 * houver mais argumentos.
 */
static arg os_Arg(arg a) {
  if (not a.beg) {
    a.end = a.beg = GetCommandLineW();
  } else {
    if (*a.end == '"') {
      a.end++;
    }
    while (*a.end == '\t' or *a.end == ' ') {
      a.end++;
    }
    a.beg = a.end;
  }

  switch (*a.beg) {
    case 0:
      a.end = a.beg = 0;
      return a;
    case '"':
      for (a.end = ++a.beg;; a.end++) {
        switch (*a.end) {
          case 0:
          case '"':
            return a;
        }
      }

    default:
      for (;; a.end++) {
        switch (*a.end) {
          case 0:
          case '\t':
          case ' ':
            return a;
        }
      }
  }
}

// Retorne o comprimento impresso de um ano (1-4).
static int tamanhoAno(int ano) {
  return 1 + (ano > 9) + (ano > 99) + (ano > 999);
}

// retorna verdadeiro caso o ano seja bissesto.
static int ehBissexto(int ano) {
  return ano <= 1582 ? (ano % 4 == 0) // Juliano
      : (ano % 4 == 0 and (ano % 100 != 0 or ano % 400 == 0)); // Gregoriano
}

// Retorna o numero de dias de determinado mes/ano.
static int diasNoMes(int ano, int mes) {
  static const unsigned char t[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return mes == 2 and ehBissexto(ano) ? 29 : t[mes - 1];
}

/* Retorna verdadeiro se a data pertencer a era Gregoriana em
 * vez da era Juliana.
 */
static int ehGregoriano(int ano, int mes, int day) {
  return ano > 1582 or (ano == 1582 and mes > 10) or (ano == 1582 and mes == 10 and day > 4);
}

/* Retorne o dia da semana (1-7, Domingo=1) para um ano,
 * mês (1-12) e dia (1-31).
 */
static int diaSemanaZeller(int ano, int mes, int day) {
  int y, c, m, era;
  ano -= (mes + 21) / 12 % 2;
  y = ano % 100;
  c = ano / 100 % 100;
  m = 3 + (9 + mes) % 12;
  era = ehGregoriano(ano, mes, day) ? c / 4 - 2 * c : 5 - c;
  return (day + 13 * (m + 1) / 5 + y + y / 4 + era + 52 * 7 + 6) % 7 + 1;
}

static wchar_t *poeCaracter(wchar_t *memo, wchar_t c) {
  memo[0] = c;
  return memo + 1;
}

static wchar_t *poeNovaLinha(wchar_t *memo) {
  while (memo[-1] == ' ') {
    memo--; // Remove espaços em branco do final
  }
  memo = poeCaracter(memo, '\r');
  memo = poeCaracter(memo, '\n');
  return memo;
}

static wchar_t *poeEspaco(wchar_t *memo, int tamanho) {
  int i;
  for (i = 0; i < tamanho; i++) {
    memo[i] = ' ';
  }
  return memo + tamanho;
}

static wchar_t *poeCadeia(wchar_t *memo, wchar_t *s, int tamanho) {
  int i;
  for (i = 0; i < tamanho; i++) {
    memo[i] = s[i];
  }
  return memo + tamanho;
}

static wchar_t *poeAno(wchar_t *memo, int ano) {
  wchar_t *p = memo;
  switch (tamanhoAno(ano)) {
    case 4: p = poeCaracter(p, '0' + ano / 1000     ); /* continua... */
    case 3: p = poeCaracter(p, '0' + ano / 100  % 10); /* continua... */
    case 2: p = poeCaracter(p, '0' + ano / 10   % 10); /* continua... */
    case 1: p = poeCaracter(p, '0' + ano        % 10);
  }
  return p;
}

static wchar_t *poeLinhaDiaSemana(wchar_t *memo, dataModo *dm) {
  int i;
  for (i = 0; i < 7; i++) {
    memo[i * 3 + 0] = dm->dias[i][0];
    memo[i * 3 + 1] = dm->dias[i][1];
    memo[i * 3 + 2] = ' ';
  }
  return memo + 20;
}

static wchar_t *poeLinhaMes(wchar_t *memo, int ano, int mes, int y) {
  int x;
  int dow = diaSemanaZeller(ano, mes, 1) - 1;
  int numDias = diasNoMes(ano, mes);
  for (x = 0; x < 7; x++) {
    int i = y * 7 + x;
    if (ano == 1582 and mes == 10 and i + 1 - dow > 4) {
      i += 10; // Mudanca do catamanhodario juliano para gregoriano.
    }
    if (i < dow or i >= numDias + dow) {
      memo[x * 3 + 0] = memo[x * 3 + 1] = memo[x * 3 + 2] = ' ';
    } else {
      int day = i - dow + 1;
      memo[x * 3 + 0] = day / 10 ? day / 10 + '0' : ' ';
      memo[x * 3 + 1] = day % 10 + '0';
      memo[x * 3 + 2] = ' ';
    }
  }
  return memo + 20;
}

// Renderizar um unico mes.
static wchar_t *poeMesUnico(wchar_t *memo, int ano, int mes, dataModo *dm) {
  int y;
  wchar_t *p = memo;
  int ytamanho = tamanhoAno(ano);
  p = poeEspaco(p, (20 - dm->tamanho[mes - 1] - 1 - ytamanho) / 2);
  p = poeCadeia(p, dm->meses[mes - 1], dm->tamanho[mes - 1]);
  p = poeCaracter(p, ' ');
  p = poeAno(p, ano);
  p = poeNovaLinha(p);
  p = poeLinhaDiaSemana(p, dm);
  p = poeNovaLinha(p);
  for (y = 0; y < 6; y++) {
    p = poeLinhaMes(p, ano, mes, y);
    p = poeNovaLinha(p);
  }
  return p;
}

// Renderiza um ano completo.
static wchar_t *poeAnoTodo(wchar_t *memo, int ano, dataModo *dm) {
  int y, my, mx;
  wchar_t *p = memo;
  p = poeEspaco(p, (64 - tamanhoAno(ano)) / 2);
  p = poeAno(p, ano);
  for (my = 0; my < 4; my++) {
    p = poeNovaLinha(p);
    for (mx = 0; mx < 3; mx++) {
      int m = my * 3 + mx;
      int tamanho = dm->tamanho[m];
      int pad = (20 - tamanho) / 2;
      p = poeEspaco(p, mx ? 2 : 0);
      p = poeEspaco(p, pad);
      p = poeCadeia(p, dm->meses[m], tamanho);
      p = poeEspaco(p, 20 - pad - tamanho);
    }
    p = poeNovaLinha(p);
    p = poeLinhaDiaSemana(p, dm);
    p = poeEspaco(p, 2);
    p = poeLinhaDiaSemana(p, dm);
    p = poeEspaco(p, 2);
    p = poeLinhaDiaSemana(p, dm);
    p = poeNovaLinha(p);
    for (y = 0; y < 6; y++) {
      for (mx = 0; mx < 3; mx++) {
        p = poeEspaco(p, mx ? 2 : 0);
        p = poeLinhaMes(p, ano, 1 + my * 3 + mx, y);
      }
      p = poeNovaLinha(p);
    }
  }
  return p;
}

/* Analisa ate um numero inteiro de 4 dígitos.
 * Retorna -1 para entrada invalida, 10000 para
 * fora do intervalo.
 */
static int analisador(arg a) {
  long v = 0;
  if (a.beg == a.end) {
    return -1;
  }
  while (a.beg < a.end) {
    if (*a.beg < '0' or *a.beg > '9') {
      return -1;
    }
    v = v * 10 + *a.beg++ - '0';
    if (v > 9999) {
      return 10000;
    }
  }
  return v;
}

void modoUso() {
  static const wchar_t modoUso[] = L"uso: cal [opcao] [[1..12] 1..9999]\n"
                               L"Opcoes:\n"
                               L"  -V  Mostra versao do programa\n"
                               L"  -h  Mostre opcoes de uso\n";
  os_EscreveUnicode(2, modoUso, sizeof(modoUso) / 2 - 1);
}

int main(void) {
  int argc = 0;
  int multi = 0;
  int ano, mes;
  dataModo dm[1];
  arg argv[4], a = {0, 0};
  wchar_t *p, memo[BUFMAX];

  // Dividi linha de comando em argc/argv*/
  for (a = os_Arg(a); a.beg; a = os_Arg(a)) {
    if (argc < 4) {
      if (*a.beg == '-') {
        switch (*++a.beg) {
          case 'V':
            os_EscreveUnicode(1, L"cal versao " VERSION L"\n", sizeof(L"cal versao " VERSION L"\n") / 2 - 1);
            return 0;
          case 'h':
            modoUso();
            return 0;
          default:
            modoUso();
            return 1;
        }
      } else {
        argv[argc++] = a;
      }
    }
  }

  os_IniciaDataModo(dm);
  switch (argc) {
    default:
      modoUso();
      return 1;
    case 0:
    case 1:
      ano = dm->ano;
      mes = dm->mes;
      break;
    case 2:
      multi = 1;
      ano = analisador(argv[1]);
      break;
    case 3:
      mes = analisador(argv[1]);
      ano = analisador(argv[2]);
  }
  if (ano < 1 or ano > 9999) {
    modoUso();
    return 1;
  }
  if (multi) {
    p = poeAnoTodo(memo, ano, dm);
  } else {
    if (mes < 1 or mes > 12) {
      modoUso();
      return 1;
    }
    p = poeMesUnico(memo, ano, mes, dm);
  }
  return not os_EscreveUnicode(1, memo, p - memo);
}
