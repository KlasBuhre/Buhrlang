#include <assert.h>

#include "NameBindings.h"
#include "Definition.h"

Binding::Binding(ReferencedEntity e) :
    referencedEntity(e),
    definition(nullptr),
    localObject(nullptr),
    methodList() {}

Binding::Binding(ReferencedEntity e, VariableDeclaration* o) :
    referencedEntity(e),
    definition(nullptr),
    localObject(o),
    methodList() {}

Binding::Binding(ReferencedEntity e, Definition* d) : 
    referencedEntity(e),
    definition(d),
    localObject(nullptr),
    methodList() {}

Binding::Binding(ReferencedEntity e, MethodDefinition* d) : 
    referencedEntity(e),
    definition(nullptr),
    localObject(nullptr),
    methodList() {

    methodList.push_back(d);
}

Binding::Binding(const Binding& other) :
    referencedEntity(other.referencedEntity),
    definition(other.definition),
    localObject(other.localObject),
    methodList(other.methodList) {}

Binding* Binding::create(ReferencedEntity e) {
    return new Binding(e);
}

Binding* Binding::create(ReferencedEntity e, VariableDeclaration* o) {
    return new Binding(e, o);
}

Binding* Binding::create(ReferencedEntity e, Definition* d) {
    return new Binding(e, d);
}

Binding* Binding::create(ReferencedEntity e, MethodDefinition* d) {
    return new Binding(e, d);
}

Binding* Binding::clone() const {
    return new Binding(*this);
}

bool Binding::isReferencingType() const {
    switch (referencedEntity) {
        case Class:
        case GenericTypeParameter:
            return true;
        default:
            return false;
    }
}

Type* Binding::getVariableType() const {
    switch (referencedEntity) {
        case Binding::LocalObject:
            return localObject->getType();
        case Binding::DataMember:
            return definition->cast<DataMemberDefinition>()->getType();
        default:
            return nullptr;
    }
}

NameBindings::NameBindings() : enclosing(nullptr) {}

NameBindings::NameBindings(NameBindings* enc) : enclosing(enc) {}

void NameBindings::copyFrom(const NameBindings& from) {
    for (auto& binding: from.bindings) {
        bindings.insert(make_pair(binding.first, binding.second->clone()));
    }
}

void NameBindings::use(const NameBindings& usedNamespace) {
    for (auto& binding: usedNamespace.bindings) {
        switch (binding.second->getReferencedEntity()) {
            case Binding::Class:
            case Binding::Method:
            case Binding::DataMember:
                bindings.insert(make_pair(binding.first,
                                          binding.second->clone()));
                break;
            default:
                break;
        }
    }
}

Binding* NameBindings::lookup(const Identifier& name) const {
    auto i = bindings.find(name);
    if (i == bindings.end()) {
        if (enclosing != nullptr) {
            return enclosing->lookup(name);
        }
        return nullptr;
    }
    return i->second;
}

Definition* NameBindings::lookupType(const Identifier& name) const {
    auto i = bindings.find(name);
    if (i == bindings.end() || !(i->second->isReferencingType())) {
        if (enclosing != nullptr) {
            return enclosing->lookupType(name);
        }
        return nullptr;
    }
    return i->second->getDefinition();
}

Binding* NameBindings::lookupLocal(const Identifier& name) const {
    auto i = bindings.find(name);
    if (i == bindings.end()) {
        return nullptr;
    }
    return i->second;
}

bool NameBindings::insertLocalObject(VariableDeclaration* localObject) {
    auto binding = Binding::create(Binding::LocalObject, localObject);
    return bindings.insert(make_pair(localObject->getIdentifier(),
                                     binding)).second;
}

void NameBindings::removeObsoleteLocalBindings() {
    for (auto i = bindings.cbegin(); i != bindings.cend(); ) {
        auto binding = i->second;
        const Identifier& nameInBindings = i->first;
        if (binding->getReferencedEntity() == Binding::LocalObject &&
            nameInBindings.compare(
                binding->getLocalObject()->getIdentifier()) != 0) {
            bindings.erase(i++);
        } else {
            ++i;
        }
    }
}

bool NameBindings::insertClass(
    const Identifier& name,
    ClassDefinition* classDef) {

    auto binding = Binding::create(Binding::Class, classDef);
    return bindings.insert(make_pair(name, binding)).second;
}

bool NameBindings::insertDataMember(
    const Identifier& name,
    DataMemberDefinition* dataMemberDef) {

    auto binding = Binding::create(Binding::DataMember, dataMemberDef);
    return bindings.insert(make_pair(name, binding)).second;
}

bool NameBindings::removeDataMember(const Identifier& name) {
    auto binding = lookupLocal(name);
    if (binding == nullptr ||
        binding->getReferencedEntity() != Binding::DataMember) {
        return false;
    }
    bindings.erase(name);
    return true;
}

bool NameBindings::insertMethod(
    const Identifier& name,
    MethodDefinition* methodDef) {

    auto binding = Binding::create(Binding::Method, methodDef);
    return bindings.insert(make_pair(name, binding)).second;
}

bool NameBindings::overloadMethod(
    const Identifier& name,
    MethodDefinition* methodDef) {

    auto binding = lookupLocal(name);
    if (binding == nullptr) {
        return insertMethod(name, methodDef);
    } else if (binding->getReferencedEntity() != Binding::Method) {
        return false;
    } else {
        binding->getMethodList().push_back(methodDef);
        return true;
    }
}

bool NameBindings::updateMethodName(
    const Identifier& oldName,
    const Identifier& newName) {

    auto binding = lookupLocal(oldName);
    if (binding == nullptr ||
        binding->getReferencedEntity() != Binding::Method) {
        return false;
    }
    bindings.erase(oldName);
    return bindings.insert(make_pair(newName, binding)).second;
}

bool NameBindings::removeLastOverloadedMethod(const Identifier& name) {
    auto binding = lookupLocal(name);
    if (binding == nullptr ||
        binding->getReferencedEntity() != Binding::Method) {
        return false;
    }
    binding->getMethodList().pop_back();
    return true;
}

bool NameBindings::insertGenericTypeParameter(
    const Identifier& name,
    GenericTypeParameterDefinition* genericTypeParameterDef) {

    auto binding =
        Binding::create(Binding::GenericTypeParameter, genericTypeParameterDef);
    return bindings.insert(make_pair(name, binding)).second;
}

bool NameBindings::insertLabel(const Identifier& label) {
    if (lookup(label) != nullptr) {
        return false;
    }
    auto binding = Binding::create(Binding::Label);
    bindings.insert(make_pair(label, binding));
    return true;
}
