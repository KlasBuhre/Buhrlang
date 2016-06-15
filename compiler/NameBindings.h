#ifndef NameBindings_h
#define NameBindings_h

#include <map>

#include "CommonTypes.h"

class Definition;
class ClassDefinition;
class MethodDefinition;
class DataMemberDefinition;
class GenericTypeParameterDefinition;
class VariableDeclaration;

class Binding {
public:

    enum ReferencedEntity {
        LocalObject,
        Class,
        Method,
        DataMember,
        GenericTypeParameter,
        Label
    };

    explicit Binding(ReferencedEntity e);
    Binding(ReferencedEntity e, VariableDeclaration* o);
    Binding(ReferencedEntity e, Definition* d);
    Binding(ReferencedEntity e, MethodDefinition* d);
    Binding(const Binding& other);

    bool isReferencingType() const;
    Type* getVariableType() const;

    ReferencedEntity getReferencedEntity() const {
        return referencedEntity;
    }

    VariableDeclaration* getLocalObject() const {
        return localObject;
    }

    Definition* getDefinition() const {
        return definition;
    }

    using MethodList = std::list<MethodDefinition*>;

    MethodList& getMethodList() {
        return methodList;
    }

    const MethodList& getMethodList() const {
        return methodList;
    }

private:
    ReferencedEntity referencedEntity;
    Definition* definition;
    VariableDeclaration* localObject;
    MethodList methodList;
};

class NameBindings {
public:
    NameBindings();
    explicit NameBindings(NameBindings* enc);

    Binding* lookup(const Identifier& name) const;
    Binding* lookupLocal(const Identifier& name) const;
    Definition* lookupType(const Identifier& name) const;
    bool insertLocalObject(VariableDeclaration* localObject);
    void removeObsoleteLocalBindings();
    bool insertClass(const Identifier& name, ClassDefinition* classDef);
    bool insertDataMember(
        const Identifier& name,
        DataMemberDefinition* dataMemberDef);
    bool removeDataMember(const Identifier& name);
    bool insertMethod(const Identifier& name, MethodDefinition* methodDef);
    bool overloadMethod(const Identifier& name, MethodDefinition* methodDef);
    bool updateMethodName(const Identifier& oldName, const Identifier& newName);
    bool removeLastOverloadedMethod(const Identifier& name);
    bool insertGenericTypeParameter(
        const Identifier& name,
        GenericTypeParameterDefinition* genericTypeParameterDef);
    bool insertLabel(const Identifier& label);
    void copyFrom(const NameBindings& from);
    void use(const NameBindings& usedNamespace);

    NameBindings* getEnclosing() const {
        return enclosing;
    }

    void setEnclosing(NameBindings* e) {
        enclosing = e;
    }

private:
    using BindingMap = std::map<Identifier, Binding*>;

    NameBindings* enclosing;
    BindingMap bindings;
};

#endif
