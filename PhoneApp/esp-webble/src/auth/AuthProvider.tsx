import { createContext, useEffect, useMemo, useState, type ReactNode } from "react";
import {
  createUserWithEmailAndPassword,
  onAuthStateChanged,
  signInWithEmailAndPassword,
  signOut,
} from "firebase/auth";
import { doc, getDoc, serverTimestamp, setDoc } from "firebase/firestore";
import { auth, db } from "../firebase";
import type { AuthContextValue, UserProfile } from "./types";

export const AuthContext = createContext<AuthContextValue | null>(null);

type AuthProviderProps = {
  children: ReactNode;
};

function generateSecurityCode() {
  const value = Math.floor(100000 + Math.random() * 900000);
  return `CUBE-${value}`;
}

function profileFromSnapshot(uid: string, data: Record<string, unknown>): UserProfile {
  return {
    uid,
    email: String(data.email ?? ""),
    name: String(data.name ?? ""),
    role: data.role === "expert" ? "expert" : "parent",
    securityCode:
      typeof data.securityCode === "string" ? data.securityCode : undefined,
    createdAt: data.createdAt,
    updatedAt: data.updatedAt,
  };
}

export function AuthProvider({ children }: AuthProviderProps) {
  const [firebaseUser, setFirebaseUser] = useState<AuthContextValue["firebaseUser"]>(null);
  const [profile, setProfile] = useState<UserProfile | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    let isActive = true;

    const unsubscribe = onAuthStateChanged(auth, async (user) => {
      if (!isActive) return;

      setFirebaseUser(user);
      setLoading(true);

      if (!user) {
        setProfile(null);
        setLoading(false);
        return;
      }

      try {
        const profileRef = doc(db, "users", user.uid);
        const profileSnap = await getDoc(profileRef);

        if (!isActive) return;

        setProfile(
          profileSnap.exists()
            ? profileFromSnapshot(user.uid, profileSnap.data())
            : null
        );
      } finally {
        if (isActive) {
          setLoading(false);
        }
      }
    });

    return () => {
      isActive = false;
      unsubscribe();
    };
  }, []);

  const value = useMemo<AuthContextValue>(
    () => ({
      firebaseUser,
      profile,
      loading,
      async login(email, password) {
        await signInWithEmailAndPassword(auth, email, password);
      },
      async signup(name, email, password, role) {
        const credential = await createUserWithEmailAndPassword(
          auth,
          email,
          password
        );
        const userProfile: UserProfile = {
          uid: credential.user.uid,
          email: credential.user.email ?? email,
          name,
          role,
          createdAt: serverTimestamp(),
          updatedAt: serverTimestamp(),
        };

        if (role === "parent") {
          userProfile.securityCode = generateSecurityCode();
        }

        await setDoc(doc(db, "users", credential.user.uid), userProfile);
        setProfile(userProfile);
      },
      async logout() {
        await signOut(auth);
      },
    }),
    [firebaseUser, loading, profile]
  );

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}
