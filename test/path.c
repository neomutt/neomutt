/**
 * @file
 * Test code for paths
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_NO_MAIN
#include "acutest.h"
#include "mutt/mutt.h"

void test_mutt_path_tidy_slash(void)
{
  static const char *tests[][2] =
  {
    { NULL,                     NULL,            },
    { "/",                      "/",             },
    { "//",                     "/",             },
    { "///",                    "/",             },
    { "/apple/",                "/apple",        },
    { "/apple//",               "/apple",        },
    { "/apple///",              "/apple",        },
    { "/apple/banana",          "/apple/banana", },
    { "/apple//banana",         "/apple/banana", },
    { "/apple///banana",        "/apple/banana", },
    { "/apple/banana/",         "/apple/banana", },
    { "/apple/banana//",        "/apple/banana", },
    { "/apple/banana///",       "/apple/banana", },
    { "//.///././apple/banana", "/apple/banana", },
    { "/apple/.///././banana",  "/apple/banana", },
    { "/apple/banana/.///././", "/apple/banana", },
    { "/apple/banana/",         "/apple/banana", },
    { "/apple/banana/.",        "/apple/banana", },
    { "/apple/banana/./",       "/apple/banana", },
    { "/apple/banana//",        "/apple/banana", },
    { "/apple/banana//.",       "/apple/banana", },
    { "/apple/banana//./",      "/apple/banana", },
    { "////apple/banana",       "/apple/banana", },
    { "/.//apple/banana",       "/apple/banana", },
  };

  char buf[64];
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    mutt_str_strfcpy(buf, tests[i][0], sizeof(buf));
    mutt_path_tidy_slash(buf);
    if (!TEST_CHECK(mutt_str_strcmp(buf, tests[i][1]) == 0))
    {
      TEST_MSG("Input:    %s", tests[i][0]);
      TEST_MSG("Expected: %s", tests[i][1]);
      TEST_MSG("Actual:   %s", buf);
    }
  }
}

void test_mutt_path_tidy_dotdot(void)
{
  static const char *tests[][2] =
  {
    { NULL,                                   NULL,                          },
    { "/",                                    "/",                           },
    { "/apple",                               "/apple",                      },
    { "/apple/banana",                        "/apple/banana",               },
    { "/..",                                  "/",                           },
    { "/apple/..",                            "/",                           },
    { "/apple/banana/..",                     "/apple",                      },
    { "/../cherry",                           "/cherry",                     },
    { "/apple/../cherry",                     "/cherry",                     },
    { "/apple/banana/../cherry",              "/apple/cherry",               },
    { "/apple/..",                            "/",                           },
    { "/apple/../..",                         "/",                           },
    { "/apple/../../..",                      "/",                           },
    { "/apple/../../../..",                   "/",                           },
    { "/apple/banana/..",                     "/apple",                      },
    { "/apple/banana/../..",                  "/",                           },
    { "/apple/banana/../../..",               "/",                           },
    { "/apple/banana/../../../..",            "/",                           },
    { "/../apple",                            "/apple",                      },
    { "/../../apple",                         "/apple",                      },
    { "/../../../apple",                      "/apple",                      },
    { "/../apple/banana/cherry/damson",       "/apple/banana/cherry/damson", },
    { "/apple/../banana/cherry/damson",       "/banana/cherry/damson",       },
    { "/apple/banana/../cherry/damson",       "/apple/cherry/damson",        },
    { "/apple/banana/cherry/../damson",       "/apple/banana/damson",        },
    { "/apple/banana/cherry/damson/..",       "/apple/banana/cherry",        },
    { "/../../apple/banana/cherry/damson",    "/apple/banana/cherry/damson", },
    { "/apple/../../banana/cherry/damson",    "/banana/cherry/damson",       },
    { "/apple/banana/../../cherry/damson",    "/cherry/damson",              },
    { "/apple/banana/cherry/../../damson",    "/apple/damson",               },
    { "/apple/banana/cherry/damson/../..",    "/apple/banana",               },
    { "/../apple/../banana/cherry/damson",    "/banana/cherry/damson",       },
    { "/apple/../banana/../cherry/damson",    "/cherry/damson",              },
    { "/apple/banana/../cherry/../damson",    "/apple/damson",               },
    { "/apple/banana/cherry/../damson/..",    "/apple/banana",               },
    { "/apple/..banana/cherry/../damson",     "/apple/..banana/damson",      },
    { "/..apple/..banana/..cherry/../damson", "/..apple/..banana/damson",    },
  };

  char buf[64];
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    mutt_str_strfcpy(buf, tests[i][0], sizeof(buf));
    mutt_path_tidy_dotdot(buf);
    if (!TEST_CHECK(mutt_str_strcmp(buf, tests[i][1]) == 0))
    {
      TEST_MSG("Input:    %s", tests[i][0]);
      TEST_MSG("Expected: %s", tests[i][1]);
      TEST_MSG("Actual:   %s", buf);
    }
  }
}

void test_mutt_path_tidy(void)
{
  static const char *tests[][2] =
  {
    { "/..apple/./../////./banana/banana/./banana/..apple/./banana/..apple/banana///banana/..apple/banana/..apple/banana/banana/..apple",                                     "/banana/banana/banana/..apple/banana/..apple/banana/banana/..apple/banana/..apple/banana/banana/..apple",                                      },
    { "/../../banana///..apple///..apple///banana///banana/banana/banana/..apple/banana/banana/banana/./banana/banana/banana/..apple/banana",                                 "/banana/..apple/..apple/banana/banana/banana/banana/..apple/banana/banana/banana/banana/banana/banana/..apple/banana",                         },
    { "///banana/banana/banana/./..apple/../banana/..apple/../..apple/./banana/./..apple",                                                                                    "/banana/banana/banana/banana/..apple/banana/..apple",                                                                                          },
    { "/./banana/banana/../banana/banana/.///banana/..apple/..apple",                                                                                                         "/banana/banana/banana/banana/..apple/..apple",                                                                                                 },
    { "/../banana/banana/banana/banana///..apple///..apple/banana/banana/////./..apple/./../.././banana/banana///banana/banana",                                              "/banana/banana/banana/banana/..apple/..apple/banana/banana/banana/banana/banana",                                                              },
    { "/banana/banana/./././..apple/banana///./banana/banana/banana/banana/banana/banana/../////banana/banana/banana/./..apple/..apple/..///..apple",                         "/banana/banana/..apple/banana/banana/banana/banana/banana/banana/banana/banana/banana/..apple/..apple",                                        },
    { "/banana///..apple///../banana/banana/banana///////banana/banana/./..apple/..apple/./..apple/..apple/banana",                                                           "/banana/banana/banana/banana/banana/banana/..apple/..apple/..apple/..apple/banana",                                                            },
    { "/banana/..apple/..apple/..apple/..apple/banana///../..apple///banana/banana/banana/banana///./../..apple/../banana/..apple/../banana/banana/./..apple",                "/banana/..apple/..apple/..apple/..apple/..apple/banana/banana/banana/banana/banana/banana/..apple",                                            },
    { "/banana/banana/..///../banana/../banana/banana/..apple/./../banana/../../banana/.",                                                                                    "/banana/banana",                                                                                                                               },
    { "/banana/banana/../..apple/banana/././banana///banana/banana",                                                                                                          "/banana/..apple/banana/banana/banana/banana",                                                                                                  },
    { "/////banana/banana/banana///..apple/./banana/..apple/./banana/banana",                                                                                                 "/banana/banana/banana/..apple/banana/..apple/banana/banana",                                                                                   },
    { "/..apple/..apple/banana///banana/././//.///./banana///./banana/..apple/./banana",                                                                                      "/..apple/..apple/banana/banana/banana/banana/..apple/banana",                                                                                  },
    { "///./..apple/banana/./../banana/././..apple///./../../../////banana/banana/../..apple/banana/banana/../banana/banana/../.",                                            "/banana/..apple/banana/banana",                                                                                                                },
    { "/banana/./../././../..apple/banana/banana/..///../.",                                                                                                                  "/..apple",                                                                                                                                     },
    { "/./..apple/banana///./banana/..///../banana//",                                                                                                                        "/..apple/banana",                                                                                                                              },
    { "/.///banana///..apple/banana/banana/../.././banana/../..apple///banana/banana/./banana/banana/..//",                                                                   "/banana/..apple/..apple/banana/banana/banana",                                                                                                 },
    { "/..apple/..apple/../banana/banana/..apple/./banana/../banana///banana",                                                                                                "/..apple/banana/banana/..apple/banana/banana",                                                                                                 },
    { "/banana/banana/../././banana/banana/banana///./.././//banana/banana/banana/.././banana///..apple/banana//",                                                            "/banana/banana/banana/banana/banana/banana/..apple/banana",                                                                                    },
    { "/banana/banana/../banana/./banana/banana/banana/..apple/../banana/.///banana/////../..apple/banana/banana/../..apple/banana/banana/banana///banana",                   "/banana/banana/banana/banana/banana/banana/..apple/banana/..apple/banana/banana/banana/banana",                                                },
    { "/./..apple/./banana///banana/./banana/..apple/banana///.///././banana",                                                                                                "/..apple/banana/banana/banana/..apple/banana/banana",                                                                                          },
    { "/./banana/..apple/banana/banana/.././.././..apple/banana/banana/..apple/.///..apple/.///banana/banana/..",                                                             "/banana/..apple/..apple/banana/banana/..apple/..apple/banana",                                                                                 },
    { "///./../..apple/banana/../banana///banana///..///..apple/../banana/../../banana/..apple/./banana/..apple/banana/..apple/banana//",                                     "/..apple/banana/..apple/banana/..apple/banana/..apple/banana",                                                                                 },
    { "/banana/../..apple/banana///////banana/banana/..apple/../banana/../..",                                                                                                "/..apple/banana/banana",                                                                                                                       },
    { "/../banana/..apple///banana/banana/..apple/..apple///banana/banana/banana///..apple/banana///../././banana/banana/banana/banana/banana/banana",                        "/banana/..apple/banana/banana/..apple/..apple/banana/banana/banana/..apple/banana/banana/banana/banana/banana/banana",                         },
    { "///..apple///.././banana/./..apple///..apple/..",                                                                                                                      "/banana/..apple",                                                                                                                              },
    { "///../..apple/./../..apple/banana/banana///..apple/banana///../banana/banana",                                                                                         "/..apple/banana/banana/..apple/banana/banana",                                                                                                 },
    { "/../banana/banana/banana/./banana/banana/banana///banana/banana/./banana/.",                                                                                           "/banana/banana/banana/banana/banana/banana/banana/banana/banana",                                                                              },
    { "/././..apple/./..apple/../banana/./..apple/banana///.././banana/banana/..",                                                                                            "/..apple/banana/..apple/banana",                                                                                                               },
    { "/..apple/..apple///banana/banana/..apple/////banana/banana/..apple///./../banana/banana/banana///banana/..apple/banana/..apple////",                                   "/..apple/..apple/banana/banana/..apple/banana/banana/banana/banana/banana/banana/..apple/banana/..apple",                                      },
    { "/..apple/banana/./banana/banana/banana/./banana/banana/../banana/../..///..apple/banana/./.././..///././../..apple/../banana/banana//",                                "/..apple/banana/banana/banana/banana/banana",                                                                                                  },
    { "/banana///../banana/../././..apple/..apple///.///banana/./banana/banana///banana/..apple/.",                                                                           "/..apple/..apple/banana/banana/banana/banana/..apple",                                                                                         },
    { "/////..apple/banana/banana/..apple/banana///banana//",                                                                                                                 "/..apple/banana/banana/..apple/banana/banana",                                                                                                 },
    { "/..apple///./banana///../../../..apple/..apple/..apple/./banana/banana",                                                                                               "/..apple/..apple/..apple/banana/banana",                                                                                                       },
    { "///banana///././..apple/banana/banana/././..apple/..apple/..apple/banana///././banana/././banana/..apple/banana/banana/../banana/./banana",                            "/banana/..apple/banana/banana/..apple/..apple/..apple/banana/banana/banana/..apple/banana/banana/banana",                                      },
    { "/banana///./banana/banana/..///./banana//",                                                                                                                            "/banana/banana/banana",                                                                                                                        },
    { "/banana/////banana/banana/..apple/..apple/////.///..///..apple/banana/banana/..apple/..apple///./banana",                                                              "/banana/banana/banana/..apple/..apple/banana/banana/..apple/..apple/banana",                                                                   },
    { "/..apple/banana///../..apple/////./..apple/./././banana/..apple",                                                                                                      "/..apple/..apple/..apple/banana/..apple",                                                                                                      },
    { "/banana/banana///banana/../../../..apple/banana///..apple/..apple/../.././banana/..apple/..apple/..///../../..",                                                       "/..apple",                                                                                                                                     },
    { "/..apple/./././../banana/..apple/banana/banana/////./..//",                                                                                                            "/banana/..apple/banana",                                                                                                                       },
    { "/../..apple/banana/..apple/banana/.././////banana/../banana/banana/..apple/..apple/banana/banana",                                                                     "/..apple/banana/..apple/banana/banana/..apple/..apple/banana/banana",                                                                          },
    { "/..apple/..apple/..apple///banana/banana/../banana/banana/banana/banana/banana/banana/..apple/.///./banana/./..apple/..apple/./..apple/banana/banana/banana/banana/.", "/..apple/..apple/..apple/banana/banana/banana/banana/banana/banana/banana/..apple/banana/..apple/..apple/..apple/banana/banana/banana/banana", },
    { "///..///banana///../..apple/..apple/.///banana/banana/..apple/..apple/banana/././..///banana",                                                                         "/..apple/..apple/banana/banana/..apple/..apple/banana",                                                                                        },
    { "/banana///banana/..apple/banana/..///.././..apple/banana///banana/banana/..apple///./..apple",                                                                         "/banana/banana/..apple/banana/banana/banana/..apple/..apple",                                                                                  },
    { "/banana/banana///.././banana/./banana/..apple/.././banana/../banana/////../banana/./banana/../..apple/banana/../banana/./..",                                          "/banana/banana/banana/banana/..apple",                                                                                                         },
    { "/banana/..apple/..apple/.././//banana/banana///.////",                                                                                                                 "/banana/..apple/banana/banana",                                                                                                                },
    { "/banana/.././banana/banana/banana/.///../banana/..",                                                                                                                   "/banana/banana",                                                                                                                               },
    { "/banana/.///..apple/../banana/banana/banana/../..apple///./banana/banana///./.",                                                                                       "/banana/banana/banana/..apple/banana/banana",                                                                                                  },
    { "/..apple/..apple///../..apple/..apple/banana/banana/////../banana/banana/////../banana/./.././banana/..apple",                                                         "/..apple/..apple/..apple/banana/banana/banana/..apple",                                                                                        },
    { "/./../banana/banana///banana/////./..apple/./..apple/../././..apple///banana",                                                                                         "/banana/banana/banana/..apple/..apple/banana",                                                                                                 },
    { "/..///banana/../banana/./..apple/..apple///././banana",                                                                                                                "/banana/..apple/..apple/banana",                                                                                                               },
    { "/banana/banana/banana/banana/banana/banana/banana/../banana/banana/banana/banana/banana/banana/..apple/../..apple/..apple",                                            "/banana/banana/banana/banana/banana/banana/banana/banana/banana/banana/banana/banana/..apple/..apple",                                         },
    { "/banana/.././banana/..///banana/..apple/banana/banana/..apple",                                                                                                        "/banana/..apple/banana/banana/..apple",                                                                                                        },
    { "/../banana/banana/../..///..apple/banana/..apple/../../..apple/banana/..apple/../banana/..apple/banana/..apple///../banana/banana/banana/../banana/..apple/banana/.",  "/..apple/..apple/banana/banana/..apple/banana/banana/banana/banana/..apple/banana",                                                            },
    { "/banana/banana/..apple/./banana/./././banana/..apple/////..apple/banana/banana/banana////",                                                                            "/banana/banana/..apple/banana/banana/..apple/..apple/banana/banana/banana",                                                                    },
    { "/..apple/banana/banana/../banana/banana/../..apple/banana/banana/./..",                                                                                                "/..apple/banana/banana/..apple/banana",                                                                                                        },
    { "/.///..apple/banana/banana/banana/../banana/banana///banana/banana///banana/banana/./..apple/..///banana/..apple/banana/banana///../banana/..apple/banana",            "/..apple/banana/banana/banana/banana/banana/banana/banana/banana/banana/..apple/banana/banana/..apple/banana",                                 },
    { "/.///./../../banana/../banana///banana/banana///banana///banana///banana",                                                                                             "/banana/banana/banana/banana/banana/banana",                                                                                                   },
    { "/banana/banana/./banana/../../../banana/././..apple/.././banana///..apple/../.",                                                                                       "/banana/banana",                                                                                                                               },
    { "///./../.././../../..apple/banana/..apple/..apple/banana///banana/..apple///../banana/../banana/././..apple/../..apple/./banana/.",                                    "/..apple/banana/..apple/..apple/banana/banana/banana/..apple/banana",                                                                          },
    { "/./../banana/banana///../banana/..apple/../../banana/banana/banana/banana/banana/../////banana/./banana//",                                                            "/banana/banana/banana/banana/banana/banana/banana",                                                                                            },
    { "/banana/./../.././../../banana/../../..apple///.///banana/banana/..apple/./banana/banana/banana/./banana/..apple/banana/..apple",                                      "/..apple/banana/banana/..apple/banana/banana/banana/banana/..apple/banana/..apple",                                                            },
    { "/..apple/.././banana/banana/banana/../../././//../../..apple/banana///../..apple/banana/././..apple///././banana",                                                     "/..apple/..apple/banana/..apple/banana",                                                                                                       },
    { "///../banana/.././banana/../..apple///banana/./../../..apple",                                                                                                         "/..apple",                                                                                                                                     },
    { "/banana/banana/banana/////../..apple/banana/////./banana///banana/..apple/banana/..apple/banana/.///banana/../../..",                                                  "/banana/banana/..apple/banana/banana/banana/..apple/banana",                                                                                   },
    { "///banana/banana/banana/..apple/banana/./..apple///./..apple/.",                                                                                                       "/banana/banana/banana/..apple/banana/..apple/..apple",                                                                                         },
    { "/./././banana/././banana///../////../banana/./../////../banana///..apple///..apple/./.././banana/..apple//",                                                           "/banana/..apple/banana/..apple",                                                                                                               },
    { "/banana/..apple/./../..apple/..apple/banana///./.././banana/./../..apple/banana/banana",                                                                               "/banana/..apple/..apple/..apple/banana/banana",                                                                                                },
    { "/..apple/..apple/..apple///////banana/banana/banana/banana/////./banana/banana/./banana///../.",                                                                       "/..apple/..apple/..apple/banana/banana/banana/banana/banana/banana",                                                                           },
    { "/..apple/../..apple///////banana/./..apple/./banana/../..apple/../../banana/banana///banana/banana/./..///.././..",                                                    "/..apple/banana/banana",                                                                                                                       },
    { "/./.././////banana/banana/..apple/././banana/banana/banana///./.",                                                                                                     "/banana/banana/..apple/banana/banana/banana",                                                                                                  },
    { "/banana/./../banana///././..apple/////banana///..///banana/banana///..apple",                                                                                          "/banana/..apple/banana/banana/..apple",                                                                                                        },
    { "/banana/../banana/../////..apple/banana///./////banana/./..apple/..apple///banana///banana/../banana///banana/..apple",                                                "/..apple/banana/banana/..apple/..apple/banana/banana/banana/..apple",                                                                          },
    { "/banana/banana/..apple/banana/./banana/banana/../banana///.",                                                                                                          "/banana/banana/..apple/banana/banana/banana",                                                                                                  },
    { "/..apple/..apple///./banana/./..apple/../..apple/./../banana/banana/..apple/././banana/..apple/////../../banana",                                                      "/..apple/..apple/banana/banana/banana/..apple/banana",                                                                                         },
    { "/..apple/..///banana///..apple/../banana/../..",                                                                                                                       "/",                                                                                                                                            },
    { "/banana///banana/banana/./banana/../../..apple/./banana/banana/.././//banana/..apple/..apple/banana/banana/.///banana/./banana/..///../..",                            "/banana/banana/..apple/banana/banana/..apple/..apple/banana",                                                                                  },
    { "/..apple/banana/..apple/.././//./..///banana///banana///../..///banana///..apple///.././../banana/../../.",                                                            "/",                                                                                                                                            },
    { "/./banana/..apple/banana/..///./banana/../../.././../../banana/banana/banana/../..apple/banana/banana/..apple/banana/banana/.",                                        "/banana/banana/..apple/banana/banana/..apple/banana/banana",                                                                                   },
    { "/../banana/banana/banana/..apple/..///./banana/..apple///../..apple/././../..apple/banana/./.././..//",                                                                "/banana/banana/banana/banana",                                                                                                                 },
    { "///banana///../../banana///.././//../banana/banana/..apple/banana///banana/banana/banana/..apple/..",                                                                  "/banana/banana/..apple/banana/banana/banana/banana",                                                                                           },
    { "/banana/../banana/././banana/..apple/./..apple///../..apple/.././////banana/./..apple/./banana",                                                                       "/banana/banana/..apple/banana/..apple/banana",                                                                                                 },
    { "/banana/./..apple/../..apple/./banana/..apple/../banana/banana/banana/banana/banana/banana/banana",                                                                    "/banana/..apple/banana/banana/banana/banana/banana/banana/banana/banana",                                                                      },
    { "/.././..apple///banana///..apple///banana/banana/banana/..apple/banana/./banana/.././banana/././/",                                                                    "/..apple/banana/..apple/banana/banana/banana/..apple/banana/banana",                                                                           },
    { "///././../banana/./../../..apple/banana/banana/..apple/banana/../..apple/..apple/./banana/./banana/..apple///banana/./..apple/banana///banana",                        "/..apple/banana/banana/..apple/..apple/..apple/banana/banana/..apple/banana/..apple/banana/banana",                                            },
    { "/..apple/banana/banana/banana///banana/..///./..apple/banana/banana/..apple/banana///.///../banana/..apple",                                                           "/..apple/banana/banana/banana/..apple/banana/banana/..apple/banana/..apple",                                                                   },
    { "/../..apple/banana/../banana/banana/banana/banana///..apple/./..apple/../..apple/..",                                                                                  "/..apple/banana/banana/banana/banana/..apple",                                                                                                 },
    { "/../banana/banana/banana/..apple/banana/../banana/banana/../../../..apple///banana/../banana",                                                                         "/banana/banana/banana/..apple/banana",                                                                                                         },
    { "/banana/..apple/..apple/../banana/banana/////../././banana/banana/..apple/..apple/.",                                                                                  "/banana/..apple/banana/banana/banana/..apple/..apple",                                                                                         },
    { "/././//banana/banana/..apple/./banana/./banana///..apple/..",                                                                                                          "/banana/banana/..apple/banana/banana",                                                                                                         },
    { "/../banana/banana///./..apple/banana/banana///.././banana/banana/.///./banana/banana/banana/banana",                                                                   "/banana/banana/..apple/banana/banana/banana/banana/banana/banana/banana",                                                                      },
    { "/banana/banana/banana/..apple/./././../..apple/banana/..apple/..apple/.///.././..",                                                                                    "/banana/banana/banana/..apple/banana",                                                                                                         },
    { "///..apple/./..apple/..apple/banana/banana/banana/../////.//",                                                                                                         "/..apple/..apple/..apple/banana/banana",                                                                                                       },
    { "/../banana/../../..apple/..apple///..apple/././banana/./banana/..apple///./..apple/./banana/banana/banana/./.././banana/../..",                                        "/..apple/..apple/..apple/banana/banana/..apple/..apple/banana",                                                                                },
    { "/..apple/..apple/banana///..apple///..apple/..apple/banana/.././banana/..apple/././..apple/../..apple///..apple///..apple/banana/../banana/..apple/////banana",        "/..apple/..apple/banana/..apple/..apple/..apple/banana/..apple/..apple/..apple/..apple/banana/..apple/banana",                                 },
    { "/../..apple/././banana///../..apple/banana/../.././////banana/banana/../..apple",                                                                                      "/..apple/banana/..apple",                                                                                                                      },
    { "/banana/..apple/banana/banana///..apple/banana/../banana/.././/",                                                                                                      "/banana/..apple/banana/banana/..apple",                                                                                                        },
    { "/..apple/banana/banana/banana/./banana/../banana/banana///..apple/banana/..///..///.",                                                                                 "/..apple/banana/banana/banana/banana/banana",                                                                                                  },
    { "/..apple/banana/banana/.././banana/..apple/banana/..apple/..apple/../..///..apple///banana/banana/banana///banana/..apple/banana/banana",                              "/..apple/banana/banana/..apple/banana/..apple/banana/banana/banana/banana/..apple/banana/banana",                                              },
    { "/./banana///../banana/banana/./../..apple/banana/../../banana///banana/..apple/..apple/////..",                                                                        "/banana/banana/banana/..apple",                                                                                                                },
    { "/banana/..apple/banana///banana///./..apple/banana/banana/banana/..apple/banana/banana//",                                                                             "/banana/..apple/banana/banana/..apple/banana/banana/banana/..apple/banana/banana",                                                             },
  };

  char buf[192];
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    mutt_str_strfcpy(buf, tests[i][0], sizeof(buf));
    mutt_path_tidy(buf);
    if (!TEST_CHECK(mutt_str_strcmp(buf, tests[i][1]) == 0))
    {
      TEST_MSG("Input:    %s", tests[i][0]);
      TEST_MSG("Expected: %s", tests[i][1]);
      TEST_MSG("Actual:   %s", buf);
    }
  }
}

void test_mutt_path_getcwd(void)
{
  // void mutt_path_getcwd(struct Buffer *cwd)

  {
    mutt_path_getcwd(NULL);
    TEST_CHECK_(1, "mutt_path_getcwd(NULL)");
  }

  {
    struct Buffer *buf = mutt_buffer_new();
    mutt_path_getcwd(buf);
    TEST_CHECK(buf->data[0] == '/');
    mutt_buffer_free(&buf);
  }
}
