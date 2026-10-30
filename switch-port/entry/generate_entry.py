#!/usr/bin/env python3
"""Extract a small real entry slice from the user's verified local PPU lift.

Generated output contains game-derived code and belongs in ignored local build
directories. Unknown later functions stop with an explicit unsupported error.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_ELF = "90eaa5e9e48b4874292badc311ccea253e6a622ff188bfbdbe76e6f6821269d8"
FUNCTIONS = {
    "000124C0": ("000", "0494b3f0d6edae11ecb78aad1275d4f7f7454e9ae8a3fa228fe13c4eddd0e78a"),
    "000125F0": ("000", "a8ab50893647620a822899c79cc6c97ce2276197229fa8ccca2d8617172eb409"),
    "016BB3E4": ("011", "7de9f6b7e45bb2c821421769ce9cd5e5a219c63c1c1f7d5bc27aa86500af67b8"),
    "00011EA0": ("012", "22f7f67fedbc6c7dd48932ee42d6ace8165ffb24715eea52d531619cac18beed"),
    "0167ACE0": ("011", "31c68a4a500e7a8db7920f51c3c0f06de76a51ee1da16f6445a67b085d0c22fc"),
    "0167A9C8": ("011", "fc66ad10c7742b90ef9c2aa4d56f4e6833b4a4f2af7475121c2b24d4cc633a6b"),
    "016BB2C4": ("011", "3420151857f9eb0151858e17fed55a35225418835c5c5fd8397c184df0229669"),
    "00010D60": ("012", "844fd38388bb11efe6cf883a12478b24d83282267fb6ebbc2f9a446b8af9b0cb"),
    "01676368": ("011", "49f2d33da3ed16c26fd5572b8713301510d1fb445767009cc0e08b08f6c8ec2d"),
    "000120C0": ("012", "9ae0560519c75cff222e596dbe85a8f1fcd9a47f818ebbe21753ec3dcc849ad3"),
    "0166FA10": ("011", "f1d759a6ae40c8389d31a9129daf21d5de4b94892894c3480856ad6da5732d24"),
    "01676258": ("011", "4bbec9fe7579125ad0c0f614d0e901c3638345c725a86b0888d1a3efab168c17"),
    "00010D50": ("012", "7e3985c842fd439451ee2cffd16b3b0ea803af1ae7bf5cd6c8635fe8f3c53768"),
    "01675CB0": ("011", "6fbd0e6d2533de9443ecd1114ef2379159b1e788479a94f1d30ce3a14f1cf722"),
    "016BB404": ("011", "841c3544bf18d61d355c6ed157161d5d14e592a31a0decdaaec7dda3093d627d"),
    "00010210": ("000", "31f17ca4c18e2dc155d4ebcf99e8cf799a1a46c7889a784331ba44882d39f870"),
    "00012878": ("000", "ef13177d801aa1db8d0cfe4cc7bfa41c9d30f6a1f162a07388ef040d74ec5ce7"),
    "00010200": ("013", "486d427fd02c0fc08c85ffe64ad3ec09eb290130d2c576fc307fe2d7163334bd"),
    "016B84B0": ("011", "1cec6a981e849ba827115d4573aa081976d9ada9ff196c1c90fc06aae41d588e"),
    "007CF848": ("003", "b6d5ffd98afd8a7a73edff44d39a9536ca957bef8e500c02d609c8b866153aa0"),
    "007CF5D0": ("003", "0a49edc99c60c1d30f358da4dbd7d1602084a4a29951e5d45b41b4b6ba83371f"),
    "00DBA100": ("007", "583054c6093353bde959c812e7233f383be2538726e87e82d31ccaefe54b8b2e"),
    "007CEE78": ("003", "562dafef4cb891def3d58f70d6eea056a972c9aa2d2b201d7c62ece4598de12a"),
    "00B0AF28": ("005", "2b6ea4baa9b9ba2c27fa71189461ef0724035bd7a889aa028b14b2e1e1d3b894"),
    "000121B0": ("012", "8781ff7a6da53caf3d26d76e9ec09737d7acf1e97a3900e331c6fccb5206f07f"),
    "01664CD8": ("011", "a710fbad582b00e87a34d07e9c450e2cdd34033e4e7e28e38733e124dd0fbe1e"),
    "0166F990": ("011", "60a2b55030978e717a5a549872d5ac022686410dbd5fb0f934a3fa7880a6caae"),
    "01676150": ("011", "853d0a172929f8b828509b5e9b78c6d7221fc81577c655b8e5a3f31051ded854"),
    "016BB2E4": ("011", "edc0b717c7e2260775658578a55f8fe3e19279b5306df9ca3fe8e341b89a09c0"),
    "016BB244": ("011", "cb6408e74e7014e102e9e2c2cb96220e4441aca7eabc08c480405f1f6c73ed5a"),
    "0166F910": ("011", "d3072b2ed3b61ce0c7d830663e81c0b95a91e77bd8c3921f492db5fa5087a6ae"),
    "01676108": ("011", "2ba235ecd31ff8f684bded4b072ffaf9ac57aa5de6c50cbb089d24e8ae95e320"),
    "016BB264": ("011", "14f16cc068d284cea10cd6f59445991c4e0deab7d9422ef260aff7db26c7a9be"),
    "00A668B0": ("005", "0ae878e87797203f4190f41133664888448e1ae57edbe73769b4de15476d99f5"),
    "00A66790": ("005", "770287c7126d381cb3a84cb55efaff728b5c4e2fd896f4a42fab9895938f1cd2"),
    "00A662C0": ("005", "16f4fee953478edfb7cf37ab080393bbc6fb97c84e9a3f260f234a559ad70304"),
    "00AE7768": ("005", "06f812a2e77bcc1511f2f58d2f4b3006d04de5fd8bea7b61233ac6d472ef8ea8"),
    "00A35DB0": ("005", "b1c80c251cf7e9a8f96fd7fcd947df1776758dc5234962b2860edc0448e0f0e0"),
    "00A35780": ("005", "5e0f4518109a1e5ff80eaa11a68974403b4573b43bdfda19f21b22f2b4717ae6"),
    "00A664E0": ("005", "dcd132f0f5731705e5408d7ea5701f494347f947fcd517464937fba624ba76c8"),
    "00B25CB8": ("005", "b02d489b37b7383078f5402d2e65d00f312d0faf5fd649f17769121fd0660f18"),
    "00AC7578": ("005", "8ae56c016114166d71b611b6fd5e81d4cb2a760f42b72a20600f8d3a1bbd0b5b"),
    "00A662A0": ("005", "aedb1f77a8e41205cfd398b177c8a2aaad262b0fffd4e6e47649b7fa6b1e44be"),
    "00B25A60": ("005", "be66c1e11508a973f622f7e2e4c7d0d06c604ff2cf8327c3f4c135bdce1f14ca"),
    "00A34BB8": ("005", "b4860abfa256dccd4b6fff6b01a9f93fefa5a02ebee8527f883eeca23cc124c3"),
    "00A34C48": ("005", "ac223d189b325c899e9dc1fa4fad26cb1814872c30aab373a298e819a122e9de"),
    "00AB64E8": ("005", "dc5f9c0430d26f03e0617d81b432c6bbcb35a4f6c006e61a1edc16597529a032"),
    "00AB6378": ("005", "190633d870111ab349c63c7bbbbaabd5935e90bdf74e70b42eac333d681eddae"),
    "00AB5CC0": ("005", "73105e54a5befb5863d71b6e2ac99f03b6e251589d8b636a333314d1c703059c"),
    "00967998": ("004", "f7aea4f32fbbb6ea76517e4bdfa199d32bb2fb53515d58c8884fb912e456b37e"),
    "00A662C8": ("005", "8a4556835518df451cad71bc5fdbf151c25836ccc133a16494e4a50884f57256"),
    "0002F0B8": ("000", "26e6db39257eae5e9efeac0d81480f30cb05d8e52c81485848f81b3ff1f1b072"),
    "00B25B58": ("005", "704a1df3fb49cbca6b72d03fa767fdc62912d4ff19e547db65e068bcf876d550"),
    "00AC7D00": ("005", "ea54a4c335db5bb4f70d3c83780815771b17002212c2b4a8d206abb6f89adb1b"),
    "00AC7AA0": ("005", "8725f21648d7ba6f21690f8148cdbddaf7567de506f29b629d75ff57a392321e"),
    "00B26C18": ("005", "b66d0517c8ba43edc37ecad353e88e7773020751615dbca2d6952f61db451dfb"),
    "00AC8578": ("005", "1e27cf1b3ba3bde79ed3ddf53f3bcced42b78439a7e10958960fab2670dde5aa"),
    "00AC8078": ("005", "a3b363244d7e9a6b17544059fa5f206214d0359aed097ffafb083bfb9ad40f54"),
    "00B25978": ("005", "b02aa044b9ea1e287ce2712656673d4728b263b101779f65a2921cd3d2ba4be7"),
    "00B25E58": ("005", "a8a549f46fa1c25ab801f669494ced9556c55025e3c28e3431410d1c95152f09"),
    "00A35210": ("005", "fef92f0384cc1f230119e5fe61a044c59f3f9cc618c1eb07366b5c108823d26f"),
    "00AB6160": ("005", "d801d01da247efd51a3792202a36601d800cef0cfc848dbb9fa1fd6a641f0c62"),
    "00AB5F90": ("005", "81e5ca64162dae644c0b1ea06657d7f9f372f4ef9a79eba751817dfa6b924950"),
    "00967AA0": ("004", "0555f158882acc9076de934a81e9a6f6d992c5650738f6bcaab90f4f1cf3dec7"),
    "00C0F580": ("006", "8034513a816ea232f97415d0940aa1ba56630a12ca09266f89690cbd942219e1"),
    "00AE4D38": ("005", "602707cff2997cc1fcf377c35a945d41f7cef98dca75023aec4f1095d110bb0b"),
    "00AE2148": ("005", "47078ce99efc2b204bd8fbed07114a67feedf270d3262d32ce982b85a395f3c9"),
    "00ADEBA0": ("005", "84087ccff2f8ece43c7027e9f3bf18f6b87d440a7a77f44c9adfafe9a32be121"),
    "007CEC40": ("003", "15d7d3c2795364bfb58f5cc7ac6ee01edec8acdd3891901a661d5b8a8a884763"),
    "007CEC48": ("003", "6706b6f126ec80d020c49d2de4398c224ddb272fc634a61df8a94e77daf7ec8d"),
    "00010BA0": ("012", "b834619481cdb0b4b1f374fc01f88ece0dc46178fd97056e8976a76259c00356"),
    "016693E0": ("011", "97e6397393f99595b825be1417e30ef8083eb33513a8bd7b10b0fa9d0a5ec850"),
    "00B27658": ("005", "cc30cfd8f66733ccef88d35e5ab9ce111dcee336a14bb65d65155d6fec216253"),
    "00ADCAB0": ("005", "7053758a6d8215c531b5daa796c03afc41cf0eed5229b17996a719009c675c4d"),
    "00AE4490": ("005", "cbf78fcf689497981aa7a6e5e39f43915a5c61939335d3c16868bc0519933ffa"),
    "007CEE00": ("003", "9fd561db2b583cdb2cfeb704ca1f6ffae6d6da4205ee5ad7e70595e00efc03b6"),
    "007CEE18": ("003", "4bbfce2e9509142f695e92bb9839f37a9e1185a51c1c463eb0d83a3a5fd87bcd"),
    "007CEE30": ("003", "fa4574dffa265bdddfd1db250b599d4df9884e171a39aa96aed975fa8591f657"),
    "007CEE48": ("003", "b87b4b402ea3e6ddf9fdd82e1c1539f7dfadd25722bb77bac1c9893c4e67336e"),
    "00AE47A0": ("005", "33a028d170e7d913412e8fc0bf3b1851d81a84b0c830fcab6fcc7d13f4da6eb1"),
    "00AE0BE8": ("005", "ba8dd8cc8227d585d5971d51accb2b9be1b7bdb6b811531cb20a977ac6765927"),
    "00782468": ("003", "af9d4596b094caac293ee68647572c0060134bd8294680d300738f7e63f35c51"),
    "00ADE478": ("005", "677fc82671e9afab04dda559d797c580955ad654d2a1d822ba9ad82e47dbf7ff"),
    "00AE42C8": ("005", "a541885ac1be5ee398b4a50ab86bed3f36334fafb747c4982c4bcce03b674485"),
    "00AE3AC8": ("005", "962ada44f10b4462a7665ab51294521fb8a689f91d082b180594144c67d77360"),
    "00A941F8": ("005", "81eb110498d1540721ec9e7865184516329d938ba037fddf9f23cd22b6f94f77"),
    "00B27730": ("005", "eba2eb9dfc075b1226ef858572951908209955d71d03cc436606d7c05e1c59aa"),
    "00AC85B8": ("005", "39b40cd304963128b848ed915272e68d42d8d54e315ae8324f5ac45e84655058"),
    "00A8CFA8": ("005", "f18a1d6a7e6a672846140096e5cf7b2cef36cc912976337c0c23063430c5bceb"),
    "00A8CE10": ("005", "c65c07577862a259aadae2d106fa68d43ea828f4ce3b58a43fc238c107af6376"),
    "00A8CA28": ("005", "d0a188c841a1e83e32aabd077428233c4791e87b2e627dc741798270eebf26fa"),
    "00AE3D68": ("005", "2f71d6ad7beb99008f099054b5e085a1169373483976e48f3c26c158d5fd270a"),
    "0066D688": ("003", "5c2b4eb2fc2e0d6e296f89d62d4a8879bcb81e50b1cc7665c7806055fd1cd5ec"),
    "0066D590": ("003", "2d006a4b80845eff78e5c9a99764f46f3b5e83686897aaacf3979d849773e092"),
    "003F4FE0": ("001", "f46bd29358fb83f688c6d959ecb6a991f84e91c553f58d4d43e915d119774e60"),
    "003F4E10": ("001", "79429946398784946c8a2d2e4ff43c4d28bee3df44162e32cdd3f3b6d49b0f8c"),
    "000CF620": ("000", "7dd3b21df433a5a8a9c6dd0cdeed9c27791dafa190e11bcf992cedd9412dbba2"),
    "003F4748": ("001", "a3172d6c62d599ecdf562c0a0f56a32cf7d35532e5e3fa94c20a81ca4de78839"),
    "003F4750": ("001", "5437b2add981f2b0e656387d9b6a16f695bf7048c055eb2c78f0bdb0f7cd41e4"),
    "003F4768": ("001", "a217d4282a72d7aebcb099419e642efbfd939869b860cc30e100844485518c05"),
    "003F4780": ("001", "67583c96e921d4a9cf5412f4e2664658c9352f4ce4ea1b9bf187099eb4c5a563"),
    "003F4798": ("001", "39b9d6efa0bc2a70377e4357ade37eec4c028e74af1db200e5004827ab2e1ba4"),
    "003F47B0": ("001", "d0422512b29d7f80d5a325b87eca16016179a43ba00a385a2153ea508e6fe78f"),
    "00ADCAD8": ("005", "f2b0294a7b2ff622149f3a83634bd8b143baeda521e5d215ee1f51d02e989062"),
    "009E54C8": ("005", "e459e98549320095c4b89a9379f5ad1f63c76fbf9aa30c94bd4009a3f03b6e7c"),
    "009E4F30": ("005", "76e5f956d7ee261d16a428cb2ffb3cd1a20035964c834365426191bf90a1b021"),
    "0095A070": ("004", "a61bb851922e7b5cc9c02b4ef9475463ad61a6106311fc561d20ff4f38dcf70b"),
    "009E3C00": ("005", "4b1128d4fb3711f11ccd16bb886551e1a29d4cd076aa123297f2d3ae80eb3d28"),
    "009E3C08": ("005", "7a05bc60b1716fd899a848907c0f84c94fe6e74699755f6fae2fe2e95cee449f"),
    "00AE60A0": ("005", "4ec2db6e796d08412b2fabfbb7534fe26f6b539347d03c55fe3eba11292a5d88"),
    "00AE1408": ("005", "537d589dee657dbbfd5d19b245009c6a0178cf48ecbcce2716e3ce8e7901c2e4"),
    "00AE5B40": ("005", "98294279272c0233c8b110ad5dd1cd37101e7aa5d2af19f1c71b979acc93fac9"),
    "00ADF018": ("005", "96b0be18cd6ec8f3c779dec870bd5dde1c73fa7cdc55ae1136247f620cfdce58"),
    "00ADCB28": ("005", "0397da524b2ccecab750d25eb603a81b2586805f9d666637983d1a8539a72b88"),
    "00ADCB50": ("005", "4ec75a0c7fd2ce3e5f3e894461b5f0a4c9d985b1706a40906d9bc460ca4fd9c7"),
    "009E3E50": ("005", "45ee4b63a7aeb0e0aa47e221e2d750b0b3cc7388676e3b53afe7cecc46a26fd8"),
    "009E3E68": ("005", "de6347c301577a6cff5deda8cc33f13f559192a34797c7fa182aed3ad9042242"),
    "009E3E80": ("005", "74e662b406aa4045d395482389cbca61cb624d9f80b715722a5be1fae3aae712"),
    "009E3E98": ("005", "940f113cca8e0f3b1521f10cf7c093a5c63b53b754fd81c3926f739e28f9d17a"),
    "01675F30": ("011", "7b68774ce64533f1aef27095493500c258a215426e8367a6550f5ae2f4a4d620"),
    "00130D38": ("000", "c9ae227d56c492e14ed1ce061a13ad96737c9fc14d548f44273286c65663063a"),
    "00130B68": ("000", "8faae056c6bfccc78e29204090a747b56a7f75894becb9fe4a7ea76cddfee5e9"),
    "0012E718": ("000", "3e6f44f05f436df21ea62b51c9690a64531fc07c5be8cb14443969705a5d7655"),
    "00130430": ("000", "7924454c691479d4dc87d19dab611b19ef2c4a3e020b5bbb48a4e0fe7ba75efd"),
    "00130438": ("000", "83cadadc58ef97d8710c8767f3ade27bc812c429536496008d6f3b6011481e2b"),
    "00130450": ("000", "8432c0130ac1ee90d030180024ed6fe4d76212f6b2f6fd85f0a9d5376c299b09"),
    "00130468": ("000", "2e5e73a7ae3512b0fa1d9eaa6ae5e25e5fd9adb4323a5ff96edb6273d18bcb51"),
    "00130480": ("000", "82b155e443bf5f6750e10a98585371e5bd4b9fe19f86999a2909f5f0bf38bb42"),
    "00130498": ("000", "d2686dd74796a6c980de9a4891ad28ae5726e54b656e5d3ec203cf662f98b614"),
    "0006B9E8": ("000", "0a04aff27f76cf95e52aac3dfc98b25fa2d9f5d5b4a5e704b691e0e890df3495"),
    "0006B818": ("000", "11e046f825b332d39e0219fe6bd2839c72088a661a4642d7604b7424cd996ba0"),
    "00065EA0": ("000", "f623ef7d5c29f1f14ff15a46a3506e9c5602bcd7795e3e2ddb741f02dc5eeb5d"),
    "0006B270": ("000", "6398bf30227878e4979c53db10df1f1e9de4d74e118f82773edb5ad35b776137"),
    "0006B278": ("000", "a39211060ace37d716542c2949ad705484b73f226a64b028e59e0360c632c578"),
    "0006B290": ("000", "4626b6d8d2386873fe77f591220ec1c7bb8d24dfa7fc8ef8198393ea56c27d91"),
    "0006B2A8": ("000", "e8dbaa021cdc1921da572e806cfb1eff86d4b766b4a88a7aab18870ead652027"),
    "0006B2C0": ("000", "bfbb5017eb3317cf5e98386dee4224f338a2779415b78ff2da040bde6b79124e"),
    "0006B2D8": ("000", "01ec3afd4bd2aca9a89dc8874680605e9088e4e329c220bdf328cab068199a19"),
    "000BF6E0": ("000", "eeb70834b868074b1aba90a7abc9b7834c2be5fd11d71bdf48c7c26ec8b031c2"),
    "000BF500": ("000", "99e0e584c2ff2a765b3d7f6f86599b3564277a550cb8fd56330b5e90f10ffbf4"),
    "000BE9D0": ("000", "188733182199bf3ea42dcb7d4877e8e4da712017725684057b609ada85f18cb3"),
    "000BE9D8": ("000", "5452917d587a88d0728c1d810cd86117c6252c8c5b042b74d4751cbdd1bfde73"),
    "000BE9F0": ("000", "4cb55cd40899b4bfa5e28e9e460a1a0ec08267756002f9497205afba789fb3dd"),
    "000BEA08": ("000", "f6c1557131b822edf57cb303de994f46052efbc174bc72e351170c0fa3e70af4"),
    "000BEA20": ("000", "a46cc39abd37a427533e978712627757f177a78e3d83004b46bf5563241072fc"),
    "000BEA38": ("000", "8975815d44df537f506a458b744bd3a13699f720419260e779a5acdb7ba9d78e"),
    "00CFCDB0": ("006", "6540e63f036243160d97d0e0337477b5535cad82f51b28404666759d97bb5717"),
    "005A9DC0": ("002", "835585802efcb524ba26e135b61dd2d33d4c1be6115e4e60bf35eb3cc3448b9b"),
    "005A9BF0": ("002", "ac411714900ba1673dc5d970c5050a16b53108124fbd26db7a6239ee23e7326f"),
    "00582130": ("002", "f207db694cb7b70e2652d65edaf35842df5bf1c9f1521af2e1fddb26c07a2856"),
    "005A97B0": ("002", "621956c3f5700b45dc6a2b7e5cce63ce1b899a3c0a19d0da36d38d224fed7f5f"),
    "005A97B8": ("002", "4fcd1aba5983e72f8af2cb92922d8184f45708eb5867111f7acf6a46865c1bdb"),
    "005A97D0": ("002", "93add1ea0d6e1b140d0db292856904e34ce0a676f8f0cb1ee0ff1b9f06c72544"),
    "005A97E8": ("002", "d4f7f9d3303f8dc0205b04bb6f4aed6e81969eaa9b0e5c28abbc89e4550452ab"),
    "005A9800": ("002", "755fd1974741d42b790ccb3a972a7c913bea9ffdbdd2a9e14203f61c31f2245c"),
    "005A9818": ("002", "d12c54b2921300ae4f2c26d6f0ec5c32a604e45e860aff3b7939eb21e2167739"),
    "008B89D0": ("004", "61e99039ce0bf198c8501748ea4b4916344f37fdb395734838ba63f05e966363"),
    "008B8600": ("004", "6b7cf0b1bc734990963ecea90cb29f3336239d220800c39e824215d906e1b1ad"),
    "008B7B68": ("004", "7fe72447888719238ab2732d2ea142e2530b73095d123ac1997424b59ef4bb50"),
    "008B7B70": ("004", "2d5bcfdab6558e1b67785e2dd9c296f3464b268740b77813580dfa821f9561a8"),
    "008B7CB8": ("004", "7cb08358d1816a9bbdcef810951e63331c8e7dacefe5fc961aaf49ce10aa5cee"),
    "008B7CD0": ("004", "e42b41b4166f7c9026e763a38bed385f0a9c6e7e221348b9d396390c1e649ed3"),
    "008B7CE8": ("004", "82b893278c6767e53104a1aadbf5af4c43c7ec0e93061eec078d3021c04690b5"),
    "008B7D00": ("004", "f0149679a2c1d6b34df771b27360b797457559f40f90fbe2e55c6174206c315a"),
    "0071AE20": ("003", "0af90cb9e8b1fa6eefb6cf6cafd537b39155c25565b846858c8a2b77032e8c97"),
    "0066A4A8": ("003", "474bc342c75ac92a9718aba4570b80a9a71fd611f70280609b55771255b96936"),
    "0066A2D8": ("003", "ee41edb559c28535e949de3e9253b808ddae95fe0531c322b9f16b274ed38339"),
    "00669B88": ("003", "bc1ff3d851e23caad0ee66bdd17e15c1e058cf43e45f420750e9f5902ac7287f"),
    "00669B90": ("003", "3aee2f89c77bcb35e2e0d1f5d5c566deb427e2699ef1019a386432cfbcc5629c"),
    "00669BA8": ("003", "9d7209c1e3bf4de35acfc4aa6279762046f544c5eeaabe2f55042216bf9055cb"),
    "00669BC0": ("003", "b80fe8b021c4e4fa26ae62f0bef2248f4615136426db5306908a1fe5022f0b2b"),
    "00669BD8": ("003", "02413caacf30072c7e0a1c12d01ebfe17e59819447aaa597369e75acd9ea38a5"),
    "00669BF0": ("003", "a690b755bcf917ee9c19d5f625053b8933b2adf94a713f50d26555c559aa9070"),
    "005720A8": ("002", "4386707d3afc7240b10182cec48b61a72150a47c2dbf8b42c65f8dc092c4eaa7"),
    "00ADF198": ("005", "6cee3ea2ba78d79dbd58629963463eb30a124d26d7e585d878d286347d876ec5"),
    "005FD720": ("003", "785b52b98cc5a304cbe758c5f3f5da80cc1eab30abf4efc0b8c4ec3d3744abd6"),
    "005FD628": ("003", "6109b9ea266491bdb643b6dd0ee41e2589cbb62c437bc34ae886582aaef372fa"),
    "003F46B0": ("001", "91fcf3c0a1c3cfc7a16877fcd7b13f723167db5e914a6a2c88e033a664e1fdad"),
    "003F44E0": ("001", "919f1fbb685383114323ce3ff29922ef8f08d833f7aa9e0cc82675950f05d81d"),
    "003F3F10": ("001", "dcadfdf66b5aaecbae78761ed42bc93278ee519a7eee84ab8f92ebe6fcf99a4a"),
    "003F3F18": ("001", "f95f201cec83eddf9628924a239d7d1e260f55548efb49a9c4b91350d4c0ea82"),
    "003F3F30": ("001", "3dcb9f0f62b3e0a8333c70ef6fd2701bd7029b631f69a53a314373a15abbeef3"),
    "003F3F48": ("001", "5e70860196b55177f9a233ae84a98684ef49f3f4a872bdb721f4f1f1ea1f5de0"),
    "003F3F60": ("001", "1f8f4d1143a9744211f40b10675b4ab2f8ca6546cf2737757ab5673913386ce5"),
    "003F3F78": ("001", "fbc82e88cebdc9c61f904650e9904f6d4df8d58cc747e9783c2ce78c4ede6f1f"),
    "00CE3DF0": ("006", "285fd5992bc8f59f84beba4309ac5b7744f7249ad0f184a67ecae84adaff54ef"),
    "015166F0": ("010", "0083dc9b0402fe6a4884514c9b7208a72574a2477d21e582ef761ea917497271"),
    "01516590": ("010", "0802eec091cc32ee23e4e987f98f15f4c0139c81a77ddcd96e27a1038d5a7285"),
    "01686CE8": ("011", "c4b136781f7a7aeef5345336e2c32490a7103b832463ef6d60eafccf3432f97a"),
    "016A0500": ("011", "80b45d77ed9f8180527ecdab83b4f430f49465133646b6036cb3121ffad2c7ba"),
    "0168ABD0": ("011", "d2a862bbaaabbe2f274d8571385be4dde44158f4b02ebd22d8e11c834164014a"),
    "0167AC88": ("011", "219debde6b1e51560e9672fb1a788ebf28f02cd20e19474ae8c6945802d53458"),
    "016896C0": ("011", "9b0740df980890aa62db4578a42cccfb4e78a28dbe58ce9bcbb492503115488e"),
    "016891F0": ("011", "caa1859ad48cdb44101e7dd8153d179b8187d5019500cfa041220858033fe987"),
    "016B7F30": ("011", "40fc4f3573cef201f0ae9174b43ec12746443ed8b6e2ac05f189620cefe528d9"),
    "01689180": ("011", "3ba35863b13414633d28aba905e396ac296e7f477bd7ff229340a5f194573e9c"),
    "01676088": ("013", "1778f7b2fcfe949cba233571ebdf5954520c627107df211082f1ccbd0decf1da"),
    "00F17448": ("007", "07d6148b93bec50d639ae137a62ca2c005345edd6d8be2abbce8d7d94e094926"),
    "00F172D0": ("007", "16ae86823bdeeaa5bcc37fe64b66dd18771257c9b0da61f55ea41e159d143009"),
    "00179118": ("000", "41533e8fcf426d3129b0164bfb207d98553871dfb31efc52dc50b57126d43734"),
    "00011720": ("012", "4c837392492b37ac209e42770febe468e252df6059ab52730ee009ccbf05d51b"),
    "01666594": ("011", "8429a58d6c94721e4a3080b2898262b930b593a962283156ec55414daae9bdc7"),
    "016B7F40": ("011", "7eb6618bde37edcf34095582e4e8d3a853d5986fe1a66a6639c5e4b4e8e9acfb"),
    "01688138": ("011", "6afcf20bc9c9c8a77005155e0a7e6b47837fd212809d6235aafacca3846b700d"),
    "01685FC8": ("011", "506be41455afae49f720158bd830ceacd1b7de34586dd96143e7caf59742fb70"),
    "01685248": ("011", "591af64de30e9ea59c82bb5998412bbfab089db502ed7b50f3cdfeec3caf05b5"),
    "01688538": ("011", "d764fd69e8d52f8c54daad934de03d3199ce972bc71d7406055391d8cb7bf69b"),
    "0168A6B0": ("011", "472b5b5e40a78544aab5b2460659bb86bdd082392c4a5be2331df528a8190b95"),
    "0167AED0": ("011", "88b0c3f278dd4c4d90c0bbbcaaedb3c87777581565ab2b557ee1ef0611673ca8"),
    "01668208": ("011", "8de7037bb0bee1704d962cdeaa2cb59a4eb17222945b4b230d38113eddad1c12"),
    "01668E50": ("011", "8d642c4a313333b30733f811f25e1f9705509642ef7fbcb2b38c36e0381c56ce"),
    "016888D8": ("011", "6afed9ff920157791091fb114064b383331790046305235c7f15451e06a34181"),
    "016884C0": ("011", "345b95e1098039017a5113043bf329ac7663070db435ea9416de801f091800f9"),
    "009E96A0": ("005", "604214d12ee81dc15fea105e242774d9283e0d50b016758be4fcec991660ead5"),
    "01684D70": ("011", "692949f143532127d12e33a84e13058c73b18b11d1a73dade806122de51e7542"),
    "0168BC68": ("011", "b981d732a366584bb02073e99132d15acff614d97e9a2a8dd39c8e3eae743ab6"),
    "0168E548": ("011", "5fa5b5a39bc57b3f01e9303d931a84a4ef689868c602a6e317e8c9aefcbf832e"),
    "0166CA50": ("011", "6ab8d82e4dd76aa6fb38ef796a99e9620c6306c9c96b084fc73d40116f50a185"),
    "0166FE88": ("011", "cd2d67e5e2e652f9f00416eed53835472d99de93ffbbda50a646779bff4f01e8"),
    "00B17510": ("005", "e12a34db8b83e1cad0cfb55b513f137212d0dfea9d9e4100ed912773ca66bb88"),
    "00010C50": ("012", "6cd77894fbe78a4859dd11826e9cbbbaa8c4bf3fcdb8aacde28aa212d8fc3b42"),
    "0166C0E8": ("011", "42c99d9b27943b5daaa49b51181b21a5ce52e9581827a0e26654f6108380ad6f"),
    "01664E60": ("011", "62ae29d23b5970103b4522258a508216f7196b2953f2fae1f97383ed10a3cbd6"),
}
EXPLORATION_FUNCTIONS = {}
CHECKPOINT_FUNCTIONS = {"00AE7768": "before singleton initializer", "00B25B58": "before manager pool initialization", "00AE4D38": "after singleton completion, before next initializer", "00AE4490": "after first name-registry insertion, before callback registry initialization", "00782468": "after AE4D38 returns and first singleton lookup, before second singleton initialization", "0168ABD0": "before first runtime operator-new allocation"}
CHECKPOINT_FUNCTIONS["0168BC68"] = "after narrow streams return, before wide initializer"
CHECKPOINT_FUNCTIONS["016896C0"] = "after backed runtime allocations, before locale consumer"
CHECKPOINT_FUNCTIONS["01676088"] = "after locale object scalar initialization, before cross-TOC string constructor"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_changed(path, data):
    if path.exists() and path.read_bytes() == data:
        return
    path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path,
                        default=ROOT / "upstream/DemonsSoulsRecomp/work/recomp")
    parser.add_argument("--elf", type=Path,
                        default=ROOT / "upstream/DemonsSoulsRecomp/work/game/EBOOT.ELF")
    parser.add_argument("--out", type=Path, default=ROOT / "switch-port/build/entry")
    parser.add_argument("--annotate-indirect-calls", action="store_true",
                        help="Attach verified original bctrl return PCs for typed dispatch")
    parser.add_argument("--include-frontier", action="store_true",
                        help="Include reviewed experimental bodies beyond the production checkpoint")
    args = parser.parse_args()
    selected_functions = dict(FUNCTIONS)
    if args.include_frontier:
        selected_functions.update(EXPLORATION_FUNCTIONS)
    elf_bytes = args.elf.read_bytes()
    elf_hash = sha(elf_bytes)
    if elf_hash != EXPECTED_ELF:
        raise SystemExit("Unsupported EBOOT: source slice is for the exact verified BLUS30443 1.00 image")
    phoff = struct.unpack_from(">Q", elf_bytes, 32)[0]
    phsize, phcount = struct.unpack_from(">HH", elf_bytes, 54)
    segments = [struct.unpack_from(">IIQQQQQQ", elf_bytes, phoff + i * phsize) for i in range(phcount)]
    tail_thunks = {}
    for name, words, target, incoming_toc, outgoing_toc in (
        ("01676088", (0xF8410028, 0x3C42FFFE, 0x38421494, 0x4B8A13B4), 0xF17448, 0x19D3BAC, 0x19B5040),
        ("00011720", (0xF8410028, 0x3C420002, 0x3842EB6C, 0x49654E68), 0x1666594, 0x19B5040, 0x19D3BAC),
        ("00010C50", (0xF8410028,0x3C420002,0x3842EB6C,0x4965B48C), 0x166C0E8, 0x19B5040, 0x19D3BAC),
    ):
        if name not in selected_functions:
            continue
        start = int(name, 16)
        observed = None
        for kind, flags, offset, address, _, filesz, _, _ in segments:
            if kind == 1 and flags & 1 and address <= start and start + 16 <= address + filesz:
                observed = struct.unpack_from(">IIII", elf_bytes, offset + start - address)
                break
        if observed != words:
            raise SystemExit(f"Cross-TOC thunk {name} differs from the reviewed four instructions")
        displacement = words[3] & 0x03FFFFFC
        if displacement & 0x02000000:
            displacement -= 0x04000000
        if start + 12 + displacement != target or words[3] & 3:
            raise SystemExit(f"Cross-TOC thunk {name} is not the reviewed relative tail branch")
        tail_thunks[name] = dict(instructions=[f"0x{w:08X}" for w in words],
                                reachable_end_exclusive=f"0x{start+16:08X}", tail_target=f"0x{target:08X}",
                                incoming_toc=f"0x{incoming_toc:08X}", outgoing_toc=f"0x{outgoing_toc:08X}",
                                neighboring_source_sequences="Retained unchanged after unconditional return; unreachable from this entry")
    sources = {}
    bodies = {}
    for name, (chunk, expected) in selected_functions.items():
        if chunk not in sources:
            sources[chunk] = (args.source_dir / f"ppu_recomp_{chunk}.cpp").read_text()
        match = re.search(r"^void func_" + name + r"\(ppu_context\* ctx\) \{\n.*?^\}",
                          sources[chunk], re.M | re.S)
        if not match or sha(match[0].encode()) != expected:
            raise SystemExit(f"Lifted func_{name} differs from the inspected instruction translation")
        bodies[name] = match[0]
    indirect_sites = {}
    if args.annotate_indirect_calls:
        map_path = ROOT / "upstream/DemonsSoulsRecomp/work/loader/EBOOT.functions.json"
        map_bytes = map_path.read_bytes()
        if sha(map_bytes) != "f8396569c426921d1d9e586fa5ff60096b9cbdfefcd8d330f971eeb7d03d7e42":
            raise SystemExit("Function ranges differ from the audited local export")
        ranges = {int(item["start"], 16): int(item["end"], 16) for item in json.loads(map_bytes)}
        spelling = "ps3_indirect_call(ctx); DRAIN_TRAMPOLINE(ctx);"
        for name, body in bodies.items():
            count = body.count(spelling)
            if not count:
                continue
            start = int(name, 16)
            end = ranges.get(start)
            if end is None or end <= start or (start | end) & 3:
                raise SystemExit(f"No valid original instruction range for {name}")
            returns = []
            for kind, flags, offset, address, _, filesz, _, _ in segments:
                if kind == 1 and flags & 1 and address <= start and end <= address + filesz:
                    for pc in range(start, end, 4):
                        if struct.unpack_from(">I", elf_bytes, offset + pc - address)[0] == 0x4E800421:
                            returns.append(pc + 4)
                    break
            if len(returns) != count:
                raise SystemExit(f"Ambiguous indirect lowering for {name}: {count} statements / {len(returns)} original bctrl instructions")
            parts = body.split(spelling)
            bodies[name] = "".join(part + (f"ds_entry_indirect_callsite(ctx, 0x{returns[i]:08X}u); DRAIN_TRAMPOLINE(ctx);"
                                           if i < count else "") for i, part in enumerate(parts))
            indirect_sites[name] = returns
    original_header = (args.source_dir / "ppu_recomp.h").read_text()
    first_decl = re.search(r"^void func_[0-9A-F]{8}\(", original_header, re.M)
    if first_decl is None:
        raise SystemExit("Unrecognized generated PPU header")
    header = original_header[:first_decl.start()]
    old_trace = "#define DS_FRAME(a) ((void)0)"
    if header.count(old_trace) != 1:
        raise SystemExit("Unrecognized no-shadow-stack trace macro")
    header = header.replace(old_trace, "#define DS_FRAME(a) ds_entry_trace_function(a)")
    header = ("// Local game-derived entry context; generated, do not distribute.\n"
              "#define PPU_VM_NO_INLINE 1\n#define DS_NO_SHADOW_STACK 1\n"
              "#define PPU_THREAD_LOCAL\n"
              "extern \"C\" void ds_entry_trace_function(unsigned int address);\n" + header)
    definitions = "\n\n".join(bodies.values())
    references = set(re.findall(r"\bfunc_([0-9A-F]{8})\b", definitions))
    unknown = sorted(references - selected_functions.keys())
    declarations = "\n".join(f"void func_{name}(ppu_context* ctx);" for name in sorted(references))
    first_function = re.search(r"^void func_[0-9A-F]{8}\(ppu_context\* ctx\) \{", sources["000"], re.M)
    if first_function is None:
        raise SystemExit("Unrecognized chunk preamble")
    preamble = sources["000"][:first_function.start()]
    # The lift stores typed function pointers in an erased slot. Convert back
    # before invocation: calling the erased void(void*) type is C++ UB even
    # when both ABIs happen to pass a pointer in the same register.
    old_invoke = "_tf((void*)(ctx));"
    if preamble.count(old_invoke) != 1:
        raise SystemExit("Unrecognized trampoline drain macro")
    preamble = preamble.replace(old_invoke,
                                "reinterpret_cast<void(*)(ppu_context*)>(_tf)(ctx);")
    output = (
        "// Contains only the user's local game-derived instruction translations.\n" +
        preamble + "\n" + declarations +
        '\nextern "C" void ds_entry_unsupported(unsigned int address);\n' +
        'extern "C" void ds_entry_checkpoint_function(unsigned int address);\n' +
        'extern "C" void ds_malloc(ppu_context* ctx);\n' +
        'extern "C" void ds_entry_indirect_callsite(ppu_context* ctx, unsigned int return_pc);\n' +
        definitions + "\n\n" +
        "\n".join(f"void func_{name}(ppu_context* ctx) {{ (void)ctx; "
                  f"{'ds_entry_checkpoint_function' if name in CHECKPOINT_FUNCTIONS else 'ds_entry_unsupported'}(0x{name}u); }}"
                  for name in unknown) + "\n" +
        'extern "C" bool ds_entry_call_lifted(unsigned int code, ppu_context* ctx) {\n    switch (code) {\n' +
        "\n".join(f"    case 0x{name}u: func_{name}(ctx); return true;" for name in selected_functions) +
        "\n    default: return false;\n    }\n}\n"
    )
    args.out.mkdir(parents=True, exist_ok=True)
    write_changed(args.out / "ppu_recomp.h", header.encode())
    write_changed(args.out / "entry_slice.cpp", output.encode())
    provenance = {
        "elf_sha256": elf_hash, "function_bodies_unmodified_from_pinned_local_lift": not args.annotate_indirect_calls,
        "original_bctrl_return_pcs": indirect_sites,
        "verified_cross_toc_tail_thunks": tail_thunks,
        "indirect_annotation_scope": "Optional callsite markers only; instruction order retained; ambiguous mappings rejected",
        "preexisting_upstream_patches": {
            "00AC8078": "NULL-pool bootstrap ds_malloc branch exists in pinned source; unsupported by this port and never taken on the accepted path",
            "0167AC88": "Preexisting ds_malloc host override; this port accepts only audited operator-new calls and returns offsets into explicitly backed budgeted guest arenas"
        },
        "functions": {name: {"source_chunk": chunk, "body_sha256": expected}
                      for name, (chunk, expected) in selected_functions.items()},
        "strict_unsupported_later_functions": unknown,
        "explicit_checkpoint_functions": CHECKPOINT_FUNCTIONS,
        "first_constructor_continuation": {"complete_body_count": len(selected_functions), "function_entries": 176,
            "next_constructor": "0x0066D688", "service": "sys_mutex_create (100); private recursive native creation only",
            "uninitialized_attribute_bytes": "pad +0x1C and seven trailing name bytes are not required zero",
            "new_indirect_dispatch": "descriptor/register, incoming LR, stack, arguments, TOC and executable backing checked against pinned ELF"},
        "checkpoints": ["before sys_initialize_tls", "after sys_initialize_tls",
                        "after sys_lwmutex_create, before LV2 syscall 352",
                        "after memory query and CRT tables, before sys_time_get_system_time",
                        "after real clock and timestamp store, before first static constructor",
                        "inside first constructor after destructor registration, before singleton initializer",
                        "inside singleton manager after native sys_rwlock_create, before caller continuation",
                        "after manager publication/unlock, before static allocator pool initialization",
                        "after pool initialization, first allocation and singleton publication; before 0x00AE4D38",
                        "after first name-registry allocation/insertion, before callback registry 0x00AE4490",
                        "after AE4D38 returns with four sorted opaque key/object pairs and third existing-pool allocation; before second singleton 0x00782468"],
        "runtime_adaptations": ["checked segmented accessors; no flat VM inline path",
                                "single-thread trampoline instead of TLS",
                                "restore original function-pointer type before trampoline invocation",
                                "function trace hook instead of shadow stack",
                                "HLE TOC save slot restored before returning to caller",
                                "argv/envp in owned writable arena",
                                "audited indirect calls restore omitted bctrl LR (0x016B84F4, 0x00A667D4)",
                                "native uncontended mutex pool; cleanup after every bounded stop",
                                "audited manager indirect-call LR (0x00A35A18, 0x00AB63B8)",
                                "native reader/writer creation registry and checked 32-byte attributes",
                                "audited manager-unlock LR 0x00A35A48 and validated static pool handoff",
                                "checked big-endian native 32-bit CAS for single guest thread",
                                "native uncontended writer acquire/unlock for private rwlock; no timed wait or reader service",
                                "audited allocation indirect-call LR 0x00B26C68 / 0x00B26CC4 and range method LR 0x00AE77E8",
                                "guest access watchdog; unsupported bootstrap allocator remains a strict stop",
                                "audited registry indirect-call LR 0x00AE21DC, 0x00ADEBF8, 0x00ADEC20, 0x00AE4E18, 0x00B276AC, 0x00B27708",
                                "original checked strlen body; no host string-length substitution",
                                "real pooled 0x2000 registry allocation and first 16-byte name-record insertion",
                                "full pinned merged 0x00010BA0 body retained; unreachable later targets remain strict boundaries",
                                "audited second-registry indirect-call LR 0x00AE4524, 0x00AE514C, 0x00AE522C, 0x00AE525C, 0x00AE528C, 0x00AE52BC",
                                "third allocation from existing pool reserves 0x1000 bytes for four opaque key/object pairs; no new guest backing arena",
                                "four original pair appends and in-function signed-key small sort; no callback execution",
                                "AE4D38 returns to its caller before the next bounded stop; alternate growth, assertion, unwind and larger-sort paths remain strict boundaries"],
    }
    write_changed(args.out / "entry-provenance.json", (json.dumps(provenance, indent=2) + "\n").encode())
    print(f"Extracted {len(selected_functions)} real functions; {len(unknown)} strict later-call boundaries: {args.out}")


if __name__ == "__main__":
    main()
