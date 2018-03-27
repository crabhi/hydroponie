module uchytka(odstup) {

    sirka = 29;
    vyska = 26;

    polomer_trubky = 25;
    polomer_uchytky = 2;

    vykus_vzadu = 2;

    sklon_trubky = atan(2/100);
    
    prumer_matky = 15;

    translate([-(polomer_trubky + 3 + odstup / 2), -sirka/2])
    difference() {
        cube([2 + odstup + polomer_trubky, sirka, vyska]);
        // tady bude trubka
        translate([0, sirka/2, 0]) 
            rotate([0, sklon_trubky, 0])
            cylinder(h=vyska *2 , r=polomer_trubky, $fn=50);
        // uchytka na pasku
        rotate([90, 0, 0]) 
            translate([polomer_trubky + 3 + polomer_uchytky, vyska / 2, -35]) 
            cylinder(h=vyska * 2, r=polomer_uchytky);
        // vykus u stojny
        translate([polomer_trubky + odstup + vykus_vzadu/2, sirka/2, vyska / 2])
            cube([vykus_vzadu, 19, vyska * 2], center=true);
        // sroub
        rotate([0, 90,0]) 
            translate([-vyska/2, sirka/2, 30]) 
            cylinder(h=100, r=4);
        // vykus pro matku
        translate([polomer_trubky + 3 + odstup - vykus_vzadu -6 - 4, 
                  sirka/2 - prumer_matky/2, 
                  vyska/2 - prumer_matky/2])
            cube([6, prumer_matky, prumer_matky + (vyska - prumer_matky) / 2 + 1]);
    }

}

uchytka(40);

translate([0, 40]) uchytka(45);
translate([70, 40]) uchytka(50);
translate([70, 0]) uchytka(55);