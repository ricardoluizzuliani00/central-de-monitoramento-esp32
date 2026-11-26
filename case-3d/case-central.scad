// =====================================================================
// CASE DA CENTRAL DE MONITORAMENTO - TAMPA COM ABAS EM "I"
// =====================================================================
// A tampa assenta por cima, como antes. Em cada ponta dela ha uma aba
// reta (o "I") que desce para dentro da case, encostada na parede curta.
// O parafuso entra pela parede lateral da case (pontas x=0 e x=L) e
// rosqueia na aba. A case nao tem furo: so uma covinha-guia para furar
// DEPOIS, com a tampa encaixada (alinhamento perfeito).
// Sem pilares de canto e sem parafusos nos cantos.
// =====================================================================

$fn = 64;

// "ambos" = case + tampa lado a lado | "corpo" | "tampa"
parte = "corpo";

// ------------------------- envelope e paredes ------------------------
L = 170;
A = 80;
P = 45;
w = 2.0;
f = 3.0;
t_tampa = 2.4;
D = P - t_tampa;

// ------------------------- garras de pressao -------------------------
gr_th = 1.0;
gr_ponta = 0.4;

// ------------------------- abas em I e parafuso ----------------------
aba_t     = 4.0;   // espessura da aba (onde a rosca pega)
aba_w     = 15;    // largura da aba (ao longo de y), centrada
aba_h     = 13;    // quanto a aba desce para dentro da case
aba_folga = 0.2;   // folga entre a aba e a parede da case
aba_cham  = 0.6;   // chanfro na ponta da aba (ajuda a encaixar)

marca_d    = 1.6;  // covinha-guia na parede
marca_prof = 0.6;
furo_trava   = false; // true = ja faz o furo no modelo
furo_trava_d = 2.5;   // M3 autoatarraxante (1,7 para M2)

// ------------------------- matriz HW-692 8x32 ------------------------
pcb_w = 128;
pcb_h = 32.1;
pcb_e = 1.6;
leds_h = 8.4;
jan_w = 127;
jan_h = 31;
mat_x = 92;
mat_y = 40;
mat_folga = 0.3;
mat_gr_dx = 45;
mat_gr_w = 12;
mat_gr_ov = 1.0;
mat_gr_hk = 1.6;

// Plastico fume
plast_e = 0.6;
plast_folga = 0.0;
plast_w = pcb_w + 2*mat_folga;
plast_h = pcb_h + 2*mat_folga;
rebaixo_prof = plast_e + plast_folga;

// Moldura da janela
mold_larg = 3.0;
mold_prof = 0.6;
cham = 1.0;

// Textos da faixa inferior
nome_texto = "SÍTIO 4 IRMÃOS";
sub_texto  = "CENTRAL DE MONITORAMENTO";
nome_fonte = "Segoe UI:style=Bold";
nome_tam   = 6.0;
sub_tam    = 3.5;
nome_esp   = 1.1;
arte_prof  = 0.6;

// ------------------------- microfone -----------------------
mic_d = 13.9;
mic_e = 1.6;
mic_espuma = 1.5;
mic_folga = 0.3;
mic_x = 12;
mic_y = 68;
mic_furo = 3.2;
mic_gr_w = 4;
mic_gr_ov = 0.55;
mic_gr_hk = 1.2;

// ------------------------- sensor de temperatura ---------------------
dht_l = 37.9;
dht_c = 16.3;
dht_e = 10.2;
dht_x = 147;
dht_y = 68;
dht_guia = 1.2;
dht_parafuso = true;
dht_furo_dx = 3.8;
dht_furo_dy = 0;
dht_poste_d = 4.4;
dht_poste_h = 7.8;
dht_furo_d = 1.8;

// ------------------------- alto-falante 3525 -------------------------
alto_c = 35;
alto_a = 25;
alto_esp = 7.0;
alto_folga = 0.2;
alto_x = 32;
alto_y = 60;
alto_gr_ov = 0.5;
alto_gr_hk = 1.2;
alto_furo_d = 1.8;
alto_furo_p = 3.2;

// ------------------------- ventilacao traseira -----------------------
vent_x = 94;
vent_y0 = 24;
vent_n = 4;
vent_comp = 54;
vent_alt = 2;
vent_pitch = 5;

// ------------------------- ESP32-S3 ----------------------------------
esp_l = 70;
esp_w = 26;
esp_x = 50;
esp_y = 40;
esp_guias = false;

// posicao do parafuso (centro da aba)
z_trava = D - aba_h/2;
y_trava = A/2;

// =====================================================================
// MÓDULOS MECÂNICOS
// =====================================================================

module garra(hw, th, hgt, ov, hk) {
    translate([-hw/2, 0, 0])
        cube([hw, th, hgt]);

    hull() {
        translate([-hw/2, -ov, hgt-hk])
            cube([hw, 0.01, gr_ponta]);

        translate([-hw/2, 0, hgt-hk])
            cube([hw, 0.01, hk]);
    }
}

module suporte_matriz() {
    xi = pcb_w/2 + mat_folga;
    yi = pcb_h/2 + mat_folga;
    h_placa = leds_h + pcb_e;

    for (sx=[-1,1])
        translate([mat_x + sx*(xi+0.8) - 0.8, mat_y - 6, f])
            cube([1.6, 12, h_placa]);

    for (sy=[-1,1])
        for (dx=[-mat_gr_dx, 0, mat_gr_dx])
            translate([mat_x + dx, mat_y + sy*yi, f])
                rotate([0,0,(sy>0)?0:180])
                    garra(mat_gr_w, gr_th, h_placa + 0.15 + mat_gr_hk, mat_gr_ov, mat_gr_hk);
}

module suporte_microfone() {
    R = mic_d/2 + mic_folga;

    for (a=[90,210,330])
        translate([mic_x, mic_y, f])
            rotate([0,0,a-90])
                translate([0, R, 0])
                    garra(mat_gr_w, gr_th, mic_espuma + mic_e + 0.15 + mic_gr_hk, mic_gr_ov, mic_gr_hk);

    for (a=[30,150,270])
        translate([mic_x + (mic_d/2-2)*cos(a) - 1.5,
                   mic_y + (mic_d/2-2)*sin(a) - 1.5, f])
            cube([3, 3, mic_espuma]);
}

module suporte_sensor() {
    hg = dht_e - 1.5;

   // for (sy=[-1,1])
     //   translate([dht_x - dht_l/2,
       //            dht_y + sy*(dht_c/2 + dht_guia/2) - dht_guia/2, f])
    //        cube([dht_l, dht_guia, hg]);

    if (dht_parafuso) {
        for (lado=[-1,1])
            translate([dht_x + lado*((dht_l-0.6)/2 - dht_furo_dx),
                       dht_y + dht_furo_dy, f])
                cylinder(d=dht_poste_d, h=dht_poste_h);
    } else {
        translate([dht_x + dht_l/2, dht_y - dht_c/2 - dht_guia, f])
            cube([dht_guia, dht_c + 2*dht_guia, hg]);
    }
}

// =====================================================================
// MOTOR DE DESENHO VETORIAL 2D
// =====================================================================

module traco_2d(p1, p2, esp=1.2) {
    hull() {
        translate(p1)
            circle(d=esp, $fn=24);

        translate(p2)
            circle(d=esp, $fn=24);
    }
}

module polilinha_2d(pts, esp=1.2) {
    for (i = [0 : len(pts)-2])
        traco_2d(pts[i], pts[i+1], esp);
}

module curva_bezier_2d(p0, p1, p2, esp=1.2, passos=18) {
    pts = [
        for (i = [0 : passos])
            let(t = i / passos, u = 1 - t)
            [
                u*u*p0[0] + 2*u*t*p1[0] + t*t*p2[0],
                u*u*p0[1] + 2*u*t*p1[1] + t*t*p2[1]
            ]
    ];

    polilinha_2d(pts, esp);
}

// =====================================================================
// 1. CAIXAS D'ÁGUA (SILOS)
// =====================================================================

module arte_silos_2d() {
    translate([11, 1.2]) {
        polilinha_2d([[0, 1], [0, 12], [4.5, 15], [9, 12], [9, 3]], 1.15);
        traco_2d([0, 12], [9, 12], 1.1);
        traco_2d([0, 9], [9, 9], 1.0);
        traco_2d([0, 6], [9, 6], 1.0);

        translate([11, 1.5]) {
            polilinha_2d([[0, 1], [0, 12], [5, 15.5], [10, 12], [10, 1]], 1.15);
            traco_2d([0, 12], [10, 12], 1.1);
            traco_2d([0, 9], [10, 9], 1.0);
            traco_2d([0, 6], [10, 6], 1.0);
        }

        translate([23, 0]) {
            polilinha_2d([[0, 3], [0, 12], [4.5, 15], [9, 12], [9, 2]], 1.15);
            traco_2d([0, 12], [9, 12], 1.1);
            traco_2d([0, 9], [9, 9], 1.0);
            traco_2d([0, 6], [9, 6], 1.0);
        }

        curva_bezier_2d([-5, 0], [16, 5], [44, 0], 1.35, 24);
    }
}

// =====================================================================
// 2. CASINHA
// =====================================================================

module arte_casa_2d() {
    translate([143, 2]) {
        polilinha_2d([[0, 7], [11, 16], [22, 7]], 1.3);
        polilinha_2d([[17, 11.5], [17, 15], [19.5, 15], [19.5, 9.5]], 1.1);
        polilinha_2d([[3, 9], [3, 0], [19, 0], [19, 9]], 1.25);

        // Porta
        polilinha_2d([[6, 0], [6, 7.5], [11, 7.5], [11, 0]], 1.1);
        translate([10, 3.5])
            circle(r=0.6, $fn=24);

        // Janela
        polilinha_2d([[13, 3], [17, 3], [17, 7], [13, 7], [13, 3]], 1.0);
        traco_2d([15, 3], [15, 7], 0.8);
        traco_2d([13, 5], [17, 5], 0.8);
    }
}

// =====================================================================
// 3. TRILHAS DE CIRCUITO ELETRÔNICO
// =====================================================================

module arte_circuitos_2d() {

    pad_d = 4.8;
    pad_i = 2.0;

    // PRIMEIRA TRILHA SUPERIOR
    polilinha_2d([
        [65.13,71.07],
        [71.48,71.07],
        [73.62,73.2],
        [116.34,73.4],
        [117.8,73.79],
        [134.3,73.79],
        [139.16,69.71],
        [151.3,69.71],
        [154.6,69.2]
    ], 1.5);

    translate([63.13,71.07])
        difference() {
            circle(d=pad_d);
            circle(d=pad_i);
        }

    // SEGUNDA TRILHA SUPERIOR
    polilinha_2d([
        [44.74,66.6],
        [75.07,66.6],
        [78.96,70.39],
        [113.91,70.39],
        [118.77,66.41],
        [148.38,66.31],
        [154.69,60.09],
        [161.63,60.17]
    ], 1.5);

    translate([42.74,66.6])
        difference() {
            circle(d=pad_d);
            circle(d=pad_i);
        }

    translate([163.63,60.19])
        difference() {
            circle(d=pad_d);
            circle(d=pad_i);
        }

    // TERCEIRA TRILHA SUPERIOR
    polilinha_2d([
        [107.37,64.46],
        [115.37,64.46],
        [116.34,63.4],
        [146.44,63.4],
        [154.69,55.53],
        [158.04,55.35]
    ], 1.5);

    translate([105.37,64.46])
        difference() {
            circle(d=pad_d);
            circle(d=pad_i);
        }

    translate([160.03,55.24])
        difference() {
            circle(d=pad_d);
            circle(d=pad_i);
        }

    // ARO CIRCULAR
    translate([158,68])
        difference() {
            circle(d=9, $fn=64);
            circle(d=6, $fn=64);
        }

    // PRIMEIRA TRILHA AO LADO DA GOTA
    translate([1,10]) {
        polilinha_2d([
            [150.3,16.11],
            [155,16.31],
            [159.53,21.17],
            [159.53,26.8],
            [160.01,29.68]
        ], 1.5);

        translate([160.34,31.65])
            difference() {
                circle(d=4.8, $fn=64);
                circle(d=2.0, $fn=64);
            }
    }

    // SEGUNDA TRILHA AO LADO DA GOTA
    translate([2,10.5]) {
        polilinha_2d([
            [148.4,11.19],
            [157.99,13.11],
            [163.24,18.74],
            [163.24,23.5],
            [163.27,23.55]
        ], 1.5);

        translate([163,25.24])
            difference() {
                circle(d=4.8, $fn=64);
                circle(d=2.0, $fn=64);
            }
    }
}

// =====================================================================
// 4. GOTA
// =====================================================================

module arte_gota_forma(R, d) {
    hull() {
        circle(r=R, $fn=40);
        translate([0,d])
            circle(r=0.5, $fn=12);
    }
}

module arte_gota_2d() {
    translate([153,35])
        difference() {
            arte_gota_forma(4.5,8.6);
            arte_gota_forma(2.3,5.4);
        }
}

// =====================================================================
// PAINEL FRONTAL
// =====================================================================

module painel_frontal_posicionado_2d() {
    arte_silos_2d();
    arte_casa_2d();
    arte_circuitos_2d();
    arte_gota_2d();
}

// =====================================================================
// CORPO PRINCIPAL
// =====================================================================

module corpo() {
    difference() {
        union() {
            difference() {
                cube([L,A,D]);
                translate([w,w,f])
                    cube([L-2*w,A-2*w,D-f+0.1]);
            }

            suporte_matriz();
            suporte_microfone();
            suporte_sensor();

            if (esp_guias)
                for (s=[-1,1])
                    translate([esp_x-esp_l/2,
                               esp_y+s*(esp_w/2+1.1)-0.8,
                               f+12])
                        cube([esp_l,1.6,2.0]);
        }

              // Rebaixo da cabeça do parafuso + covinha-guia (pontas x=0 e x=L)
        cab_d    = 6.2;   // M3 cabeça panela (para M2 use 4.2)
        cab_prof = 1.0;   // deixa 1 mm de parede embaixo da cabeça

        translate([-0.01, y_trava, z_trava])
            rotate([0,90,0]) {
                cylinder(d=cab_d, h=cab_prof+0.01, $fn=48);
                cylinder(d=marca_d, h=cab_prof+0.4, $fn=24);
            }

        translate([L+0.01, y_trava, z_trava])
            rotate([0,-90,0]) {
                cylinder(d=cab_d, h=cab_prof+0.01, $fn=48);
                cylinder(d=marca_d, h=cab_prof+0.4, $fn=24);
            }
            
        // Furo de trava ja pronto (opcional)
        if (furo_trava) {
            translate([-0.1, y_trava, z_trava])
                rotate([0,90,0])
                    cylinder(d=furo_trava_d, h=w+0.2, $fn=24);

            translate([L+0.1, y_trava, z_trava])
                rotate([0,-90,0])
                    cylinder(d=furo_trava_d, h=w+0.2, $fn=24);
        }

        // Janela da matriz
        translate([mat_x-jan_w/2,mat_y-jan_h/2,-0.1])
            cube([jan_w,jan_h,f+0.2]);

        // Moldura da janela
        translate([mat_x-jan_w/2-cham-mold_larg,
                   mat_y-jan_h/2-cham-mold_larg, -0.01])
            cube([jan_w+2*(cham+mold_larg),
                  jan_h+2*(cham+mold_larg),
                  mold_prof+0.01]);

        hull() {
            translate([mat_x-jan_w/2-cham,
                       mat_y-jan_h/2-cham,
                       mold_prof])
                cube([jan_w+2*cham, jan_h+2*cham, 0.01]);

            translate([mat_x-jan_w/2,
                       mat_y-jan_h/2,
                       mold_prof+cham])
                cube([jan_w, jan_h, 0.01]);
        }

        // ESCRITA PRINCIPAL
        translate([L / 2.4, 11.5, -0.01])
            mirror([1,0,0])
                linear_extrude(height=arte_prof+0.01)
                    text(nome_texto, size=nome_tam,
                         font=nome_fonte,
                         halign="center",
                         valign="center",
                         spacing=nome_esp);

        // SUBTÍTULO
        translate([L / 2.4, 4.5, -0.01])
            mirror([1,0,0])
                linear_extrude(height=arte_prof+0.01)
                    text(sub_texto, size=sub_tam,
                         font=nome_fonte,
                         halign="center",
                         valign="center",
                         spacing=nome_esp);

        // GRAVAÇÃO FRONTAL
        translate([L, 0, -0.01])
            mirror([1,0,0])
                linear_extrude(height=arte_prof+0.01)
                    painel_frontal_posicionado_2d();

        // Cavidade interna para o plástico fumê
        translate([mat_x-plast_w/2,
                   mat_y-plast_h/2,
                   f-rebaixo_prof])
            cube([plast_w,plast_h,rebaixo_prof+0.1]);

        // Furo acústico do microfone
        translate([mic_x,mic_y,-0.1])
            cylinder(d=mic_furo,h=f+0.2);

        // Entradas de ar do sensor
        for (i=[0:4])
            translate([dht_x + (i-2)*4 - 1,
                       dht_y - 5, -0.1])
                cube([2, 10, f+0.2]);

        // Furo piloto do poste do sensor
        if (dht_parafuso)
            for (lado=[-1,1])
                translate([dht_x + lado*((dht_l-0.6)/2 - dht_furo_dx),
                           dht_y + dht_furo_dy,
                           f+1])
                    cylinder(d=dht_furo_d, h=dht_poste_h);
    }
}

// =====================================================================
// TAMPA TRASEIRA
// =====================================================================

module suporte_alto_falante() {
    Sx = alto_c + 2*alto_folga;
    Sy = alto_a + 2*alto_folga;
    pw = min(Sy*0.3, 6);

    for (sy=[-1,1])
        for (dx=[-alto_c*0.22, alto_c*0.22])
            translate([dx, sy*Sy/2, 0])
                rotate([0,0,(sy>0)?0:180])
                    garra(8, gr_th,
                          alto_esp + 0.15 + alto_gr_hk,
                          alto_gr_ov, alto_gr_hk);

    for (sx=[-1,1])
        translate([sx*(Sx/2+0.6) - 0.6, -pw, 0])
            cube([1.2, 2*pw, alto_esp*0.8]);
}

// Aba em "I": placa reta que desce da face interna da tampa
module aba_I(x0) {
    hull() {
        translate([x0, (A - aba_w)/2, t_tampa - 0.01])
            cube([aba_t, aba_w, aba_h - aba_cham]);

        translate([x0 + aba_cham, (A - aba_w)/2 + aba_cham, t_tampa + aba_h - 0.01])
            cube([aba_t - 2*aba_cham, aba_w - 2*aba_cham, 0.01]);
    }
}

module tampa() {
    nx = min(10, floor((alto_c*0.7 - alto_furo_d)/alto_furo_p) + 1);
    ny = min(8, floor((alto_a*0.7 - alto_furo_d)/alto_furo_p) + 1);

    difference() {
        union() {
            cube([L,A,t_tampa]);

            // aro-guia que centraliza a tampa na boca da case
            translate([w+0.2,w+0.2,t_tampa])
                difference() {
                    cube([L-2*w-0.4,A-2*w-0.4,3]);

                    translate([1.6,1.6,-0.1])
                        cube([L-2*w-3.6,A-2*w-3.6,3.2]);
                }

            // abas em I, uma em cada ponta, encostadas nas paredes curtas
            aba_I(w + aba_folga);
            aba_I(L - w - aba_folga - aba_t);

            translate([alto_x,alto_y,t_tampa])
                suporte_alto_falante();
        }

        for (i=[0:nx-1])
            for (j=[0:ny-1])
                translate([alto_x + (i-(nx-1)/2)*alto_furo_p,
                           alto_y + (j-(ny-1)/2)*alto_furo_p,
                           -0.1])
                    cylinder(d=alto_furo_d, h=t_tampa+0.2);

        for (i=[0:vent_n-1])
            translate([vent_x,vent_y0+i*vent_pitch,-0.1])
                cube([vent_comp,vent_alt,t_tampa+0.2]);
    }
}

// =====================================================================
// RENDERIZAÇÃO
// =====================================================================

if (parte == "corpo")
    corpo();
else if (parte == "tampa")
    tampa();
else {
    corpo();
    translate([0, A + 12, 0])
        tampa();
}