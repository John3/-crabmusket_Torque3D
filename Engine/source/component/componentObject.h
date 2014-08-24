#ifndef _COMPONENT_OBJECT_H_
#define _COMPONENT_OBJECT_H_

#ifndef _GAMEBASE_H_
#include "T3D/gameBase/gameBase.h"
#endif
#ifndef _ICALLMETHOD_H_
#include "console/ICallMethod.h"
#endif
#ifndef _CONSOLEINTERNAL_H_
#include "console/consoleInternal.h"
#endif

#ifndef _COMPONENT_H_
#include "component/components/component.h"
#endif
#ifndef _COMPONENTINSTANCE_H_
#include "component/components/componentInstance.h"
#endif

#ifndef _PLATFORM_THREADS_MUTEX_H_
#include "platform/threads/mutex.h"
#endif
#ifndef _STRINGFUNCTIONS_H_
#include "core/strings/stringFunctions.h"
#endif
#ifndef _SIMSET_H_
#include "console/simSet.h"
#endif
#ifndef _TVECTOR_H_
#include "core/util/tVector.h"
#endif
#ifndef _STRINGTABLE_H_
#include "core/stringTable.h"
#endif
#ifndef _SIMOBJECT_H_
#include "console/simObject.h"
#endif

#include "core/util/safeDelete.h"

#ifdef TORQUE_DEBUG
#ifndef _TDICTIONARY_H_
#include "core/util/tDictionary.h"
#endif
#endif

class Component;

class ComponentObject : public GameBase, public ICallMethod
{
   typedef GameBase Parent;
   friend class ComponentInstance;

   //////////////////////////////////////////////////////////////////////////
   // ICallMethod stuff
   // Allows us to handle console method calls
#ifdef TORQUE_DEBUG
   typedef Map<StringTableEntry, S32> callMethodMetricType;

   // Call Method Debug Stat.
   callMethodMetricType mCallMethodMetrics;
#endif

protected:
   /// Internal callMethod : Actually does component notification and script method execution
   ///  @attention This method does some magic to the argc argv to make Con::execute act properly
   ///   as such it's internal and should not be exposed or used except by this class
   virtual const char* _callMethod( U32 argc, const char *argv[], bool callThis = true );

   virtual const char *_callComponentMethod( U32 argc, const char *argv[], bool callThis /* = true  */ );

public:

#ifdef TORQUE_DEBUG
   /// Call Method Metrics.
   const callMethodMetricType& getCallMethodMetrics( void ) const { return mCallMethodMetrics; };

   /// Inject Method Call.
   void injectMethodCall( const char* method );
#endif

   /// Call Method
   virtual const char* callMethodArgList( U32 argc, const char *argv[], bool callThis = true );

   /// Call Method format string
   const char* callMethod( S32 argc, const char* methodName, ... );

   //////////////////////////////////////////////////////////////////////////
   // DynamicConsoleMethodComponent Overrides
   virtual bool handlesConsoleMethod( const char *fname, S32 *routingId );

   //////////////////////////////////////////////////////////////////////////
   // Behavior stuff
   // These are the functions that handle behavior management
private:
   SimSet mComponents;
   Vector<ComponentInstance*> mToLoadComponents;
   bool   mStartComponentUpdate;
   bool   mDelayUpdate;
   bool   mLoadedComponents;

protected:
   enum MaskBits 
   {
      ComponentsMask				 = Parent::NextFreeMask << 0,
      NextFreeMask             = Parent::NextFreeMask << 1
   };

   // [neo, 5/10/2007 - #3010]
   bool  mInComponentCallback; ///<  

   /// Should adding of behaviors be deferred from onAdd() to be called by derived classe manually?
   virtual bool deferAddingBehaviors() const { return false; }

public:
   virtual void write( Stream &stream, U32 tabStop, U32 flags = 0  );

   // [neo, 5/11/2007]
   // Refactored onAdd() code into this method so it can be deferred by derived classes.
   /// Attach behaviors listed as special fields (derived classes can override this and defer!)
   virtual void addComponents();

   //////////////////////////////////////////////////////////////////////////
   /// Behavior interface  (Move this to protected?)
   virtual bool addComponent( ComponentInstance *bi );
   virtual bool removeBehavior( ComponentInstance *bi, bool deleteBehavior = true );
   virtual const SimSet &getAllBehaviors() const { return mComponents; };
   virtual ComponentInstance *getComponent( StringTableEntry behaviorTemplateName );
   virtual ComponentInstance *getComponent( S32 index );
   virtual ComponentInstance *getComponentByType( StringTableEntry behaviorTypeName );
   virtual ComponentInstance *getComponent( Component *bTemplate );

   struct ComponentSelection
   {
      /// A Predicate is some check on an object to see whether it should be
      /// included in the iteration.
      struct Predicate { virtual bool check(SimSet::iterator) const = 0; };
      /// An iterator keeps a list of predicates that it should test all objects
      /// against.
      Vector<Predicate*> predicates;

      /// Check that an object is of a certain class by attempting to cast to
      /// the target class. Kind of hacky but ah well. It also allows you to
      /// store a reference to the casted object.
      template <class T> struct DynamicCastPredicate : public Predicate
      {
         T** handle;
         DynamicCastPredicate(T** h = NULL) : handle(h) {}
         bool check(SimSet::iterator bi) const
         {
            T* t = dynamic_cast<T*>(*bi);
            if(t)
            {
               if(handle)
                  *handle = t;
               return true;
            }
            return false;
         }
      };

      /// Add a DynamicCastPredicate to this iterator.
      template <class T> ComponentSelection& with(T** handle)
      {
         predicates.push_back(new DynamicCastPredicate<T>(handle));
         return *this;
      }

      /// Dynamic cast to ComponentInstance and check whether the instance is
      /// enabled.
      /*struct EnabledCheckPredicate : public Predicate
      {
         bool check(SimSet::iterator bi) const
         {
            ComponentInstance *c = dynamic_cast<ComponentInstance*>(*bi);
            return c && c->isEnabled();
         }
      };*/

      /// Check all predicates against a particular object.
      bool check(SimSet::iterator bi) const
      {
         for(U32 i = 0; i < predicates.size(); i++)
         {
            if(!predicates[i]->check(bi))
               return false;
         }
         return true;
      }

      SimSet& set;
      ComponentSelection(SimSet &s, bool includeDisabled) : set(s)
      {
         /*if(!includeDisabled)
            predicates.push_back(new EnabledCheckPredicate());*/
      }

      struct Iterator
      {
         SimSet::iterator it, end;
         const ComponentSelection& sel;
         Iterator(const ComponentSelection& s, SimSet::iterator i, SimSet::iterator e)
            : sel(s), it(i), end(e)
         {
            if(it != end && !sel.check(it))
               (*this)++;
         }

         Iterator& operator++ (int) { return ++(*this); }
         Iterator& operator++ ()
         {
            if(it != end)
            {
               while(true)
               {
                  if(++it != end)
                  {
                     if(sel.check(it))
                        break;
                  }
                  else
                     break;
               }
            }
            return *this;
         }

         /// Check for equality of iterators, and referential equality of
         /// parent selections. This is an optimisation, so beware if you try
         /// to do anything fancy that retains iterators.
         bool operator!= (const Iterator& other)
         {
            return it != other.it || &sel != &other.sel;
         }
      };

      /// An iterator at the beginning of this selection. Note that the
      Iterator begin()
      {
         return Iterator(*this, set.begin(), set.end());
      }

      Iterator end()
      {
         return Iterator(*this, set.end(), set.end());
      }
   };

   /// Return a new iterator over this object's list of components.
   ComponentSelection selectComponents(bool includeDisabled = true)
   {
      return ComponentSelection(mComponents, includeDisabled);
   }

   //template <class T>
   //static void findComponentByType( Simset* objectsList, Vector<T*> &foundObjects );
   template <class T>
   T *getComponent();
   template <class T>
   Vector<T*> getComponents();

   virtual void clearComponents(bool deleteBehaviors = true);
   virtual bool reOrder( ComponentInstance *obj, U32 desiredIndex /* = 0 */ );
   virtual U32 getComponentCount() const { return mComponents.size(); }
   void updateComponents();
   void setComponentsDirty();
   /// This forces the network update of a single specific behavior
   /// The force update arg is for special-case behaviors that are explicitly networked to certain clients
   /// Such as the camera, and it will override the check if the template defines if it's networked or not
   void setComponentDirty(ComponentInstance *bI, bool forceUpdate = false);
   void callOnComponents( String function );

public:
   ComponentObject();
   ~ComponentObject(){}

   bool onAdd();
   void onRemove();

   virtual void onPostAdd();

   virtual void addObject( SimObject* object );

   Box3F getObjectBox() { return mObjBox; }
   MatrixF getWorldToObj() { return mWorldToObj; }
   MatrixF getObjToWorld() { return mObjToWorld; }

   virtual void setObjectBox(Box3F objBox){}
   virtual void setWorldBox(Box3F wrldBox) { mWorldBox = wrldBox; }

   bool areBehaviorsLoaded() { return mLoadedComponents; }

   U32 packUpdate(NetConnection *con, U32 mask, BitStream *stream);
   void unpackUpdate(NetConnection *con, BitStream *stream);

   S32 isComponentPackable(NetConnection *con, ComponentInstance* bI);

   DECLARE_CONOBJECT(ComponentObject);
};

template< class T >
void findComponentByType( SimSet* set, Vector<T*> &foundObjects )
{
   T *curObj;
   ComponentInstance* compInst;

   set->lock();

   // Loop through our child objects.
   SimObjectList::iterator itr = set->begin();   

   for ( ; itr != set->end(); itr++ )
   {
      compInst =  dynamic_cast<ComponentInstance*>( *itr );
      if(!compInst->isEnabled())
         continue;

      curObj = dynamic_cast<T*>( *itr );

      // Add this child object if appropriate.
      if ( curObj )
         foundObjects.push_back( curObj );      
   }

   set->unlock();
}

template <class T>
T *ComponentObject::getComponent()
{
   for( SimSet::iterator b = mComponents.begin(); b != mComponents.end(); b++ )
   {
      T* t = dynamic_cast<T *>(*b);
      if(t) {
         return t;
      }
   }
   return NULL;
}

template <class T>
Vector<T*> ComponentObject::getComponents() {
   Vector<T*> v;
   findComponentByType<T>(&mComponents, v);
   return v;
}
#endif