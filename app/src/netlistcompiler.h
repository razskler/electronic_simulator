#pragma once

#include <QHash>
#include <QString>
#include <QVector>

#include "ec/circuit.h"
#include "wireitem.h"

class SchematicScene;

/* Result of compiling the schematic scene into a core EcCircuit.
 * Owns the circuit; netsByComp maps component uid -> per-pin net id
 * (0 = ground). */
struct Compiled
{
    EcCircuit *cir = nullptr;
    QHash<uint64_t, QVector<int>> netsByComp;
    QHash<WireItem *, int> netByWire;
    QHash<uint64_t, QString> nameByUid;
    QHash<uint64_t, const EcPlugin *> pluginByUid;

    ~Compiled() { reset(); }
    void reset()
    {
        if (cir) {
            ec_circuit_free(cir);
            cir = nullptr;
        }
        netsByComp.clear();
        netByWire.clear();
        nameByUid.clear();
    }
};

/* Builds the union-find nets from wires, assigns node ids (ground = 0),
 * and constructs the EcCircuit. Returns false and fills *error on failure. */
bool compileCircuit(SchematicScene *scene, Compiled *out, QString *error);
