module Samd21 {
    enum PeripheralFunction : U8 {
        A = 0,
        B = 1,
        C = 2,
        D = 3,
        E = 4,
        F = 5,
        G = 6,
        H = 7
    }

    module Pins {
        @ Pin assignments for SAM D21E/DA1E (32-pin) variants
        @ Based on Table 7-1 SAM D21J and SAM DA1J pinout
        enum E : U8 {
            PA00 = 1
            PA01 = 2
            PA02 = 3
            PA03 = 4
            PA04 = 5
            PA05 = 6
            PA06 = 7
            PA07 = 8
            PA08 = 11
            PA09 = 12
            PA10 = 13
            PA11 = 14
            PA14 = 15
            PA15 = 16
            PA16 = 17
            PA17 = 18
            PA18 = 19
            PA19 = 20
            PA22 = 21
            PA23 = 22
            PA24 = 23
            PA25 = 24
            PA27 = 25
            PA28 = 27
            PA30 = 31
            PA31 = 32
            PB02 = 33
            PB03 = 34
            PB22 = 45
            PB23 = 46
        }

            @ Pin assignments for SAM D21G/DA1G (48-pin) variants
        @ Based on Figure 5.2.1 QFN48/TQFP48 pinout
        enum G : U8 {
            PA00 = 1
            PA01 = 2
            PA02 = 3
            PA03 = 4
            PA04 = 9
            PA05 = 10
            PA06 = 11
            PA07 = 12
            PA08 = 13
            PA09 = 14
            PA10 = 15
            PA12 = 21
            PA13 = 22
            PA14 = 23
            PA15 = 24
            PA16 = 25
            PA17 = 26
            PA18 = 27
            PA19 = 28
            PA20 = 29
            PA21 = 30
            PA22 = 31
            PA23 = 32
            PA24 = 33
            PA25 = 34
            PA27 = 39
            PA28 = 41
            PA30 = 45
            PA31 = 46
            PB02 = 47
            PB03 = 48
            PB08 = 7
            PB09 = 8
            PB10 = 18
            PB11 = 19
            PB12 = 20
            PB22 = 38
            PB23 = 37
        }

        @ Pin assignments for SAM D21J/DA1J (64-pin) variants
        @ Based on Figure 5.1.1 QFN64/TQFP64 pinout
        enum J : U8 {
            PA00 = 1
            PA01 = 2
            PA02 = 3
            PA03 = 4
            PB04 = 5
            PB05 = 6
            PB06 = 9
            PB07 = 10
            PB08 = 11
            PB09 = 12
            PA04 = 13
            PA05 = 14
            PA06 = 15
            PA07 = 16
            PA08 = 17
            PA09 = 18
            PA10 = 19
            PA11 = 20
            PB10 = 21
            PB11 = 22
            PB12 = 23
            PB13 = 24
            PB14 = 25
            PB15 = 26
            PA12 = 29
            PA13 = 30
            PA14 = 31
            PA15 = 32
            PA16 = 35
            PA17 = 36
            PA18 = 37
            PA19 = 38
            PB16 = 39
            PB17 = 40
            PA20 = 41
            PA21 = 42
            PA22 = 43
            PA23 = 44
            PA24 = 45
            PA25 = 46
            PB22 = 49
            PB23 = 50
            PA27 = 51
            PA28 = 53
            PA30 = 57
            PA31 = 58
            PB30 = 59
            PB31 = 60
            PB00 = 61
            PB01 = 62
            PB02 = 63
            PB03 = 64
        }
    }
}
