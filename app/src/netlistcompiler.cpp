#include "netlistcompiler.h"

#include "schematicscene.h"

#include <QGraphicsItem>

namespace {

struct UnionFind {
    QHash<quint64, int> id;
    QList<int> parent;

    int make()
    {
        parent.append(parent.size());
        return parent.size() - 1;
    }
    int find(int x)
    {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }
    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b)
            parent[b] = a;
    }
};

} // namespace

bool compileCircuit(SchematicScene *scene, Compiled *out, QString *error)
{
    out->reset();

    QList<ComponentItem *> comps = scene->componentItems();
    QList<WireItem *> wires = scene->wireItems();
    if (comps.isEmpty()) {
        *error = QStringLiteral("The schematic is empty - drag components in from the palette.");
        return false;
    }

    /* key: (uid << 4) | pin; uid 0 is reserved for the ground sentinel */
    UnionFind uf;
    QHash<quint64, int> nodeOf;

    auto nodeId = [&](quint64 key) -> int {
        auto it = nodeOf.find(key);
        if (it != nodeOf.end())
            return it.value();
        int n = uf.make();
        nodeOf.insert(key, n);
        return n;
    };

    bool hasGround = false;
    const int groundKey = 0; /* (0<<4)|0 */
    int groundNode = -1;

    for (ComponentItem *ci : comps) {
        for (int p = 0; p < ci->plugin()->n_pins; p++)
            nodeId((quint64(ci->uid()) << 4) | quint64(p));
        if (ci->plugin()->id && strcmp(ci->plugin()->id, "ground") == 0) {
            hasGround = true;
            groundNode = nodeId((quint64(ci->uid()) << 4) | 0);
            uf.unite(groundNode, nodeId(groundKey));
        }
    }
    if (!hasGround) {
        *error = QStringLiteral("No ground component in the circuit - add at least one.");
        return false;
    }
    nodeId(groundKey); /* ensure ground sentinel exists */

    for (WireItem *w : wires) {
        if (!w->a.comp || !w->b.comp)
            continue;
        uf.unite(nodeId((quint64(w->a.comp->uid()) << 4) | quint64(w->a.pin)),
                 nodeId((quint64(w->b.comp->uid()) << 4) | quint64(w->b.pin)));
    }

    /* assign net ids: ground root = 0, others sequentially */
    QHash<int, int> netOfRoot;
    netOfRoot.insert(uf.find(nodeId(groundKey)), 0);
    int nextNet = 1;

    out->cir = ec_circuit_new();
    for (ComponentItem *ci : comps) {
        const EcPlugin *p = ci->plugin();
        QVector<int> nets;
        nets.reserve(p->n_pins);
        for (int pin = 0; pin < p->n_pins; pin++) {
            int root = uf.find(nodeOf.value((quint64(ci->uid()) << 4 | quint64(pin))));
            if (!netOfRoot.contains(root))
                netOfRoot.insert(root, nextNet++);
            nets.append(netOfRoot.value(root));
        }
        EcComp *ec = ec_circuit_add(out->cir, p, ci->name().toUtf8().constData());
        ec->uid = ci->uid();
        for (int pin = 0; pin < p->n_pins && pin < EC_MAX_PINS; pin++)
            ec->nodes[pin] = nets[pin];
        for (int i = 0; i < p->n_params && i < EC_MAX_PARAMS; i++)
            ec->params[i] = ci->param(i);
        out->netsByComp.insert(ci->uid(), nets);
        out->nameByUid.insert(ci->uid(), ci->name());
        out->pluginByUid.insert(ci->uid(), p);
    }
    ec_circuit_finalize(out->cir);

    for (WireItem *w : wires) {
        if (!w->a.comp || !w->b.comp)
            continue;
        QVector<int> nets = out->netsByComp.value(w->a.comp->uid());
        if (w->a.pin < nets.size())
            out->netByWire.insert(w, nets[w->a.pin]);
    }
    return true;
}
